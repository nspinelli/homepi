#!/usr/bin/env bash
# Installs homepi-shairport-sync: upstream shairport-sync, supervisor, systemd units.
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_NAME="homepi-shairport-supervisor"
INSTALL_ROOT="/opt/homepi/services/shairport"
BUILD_DIR="${SERVICE_ROOT}/build"
SRC_DIR="${BUILD_DIR}/shairport-sync-src"
SUPERVISOR_BUILD="${BUILD_DIR}/supervisor"
SUPERVISOR_UNIT="${SERVICE_ROOT}/systemd/${SERVICE_NAME}.service"
ZONE_UNIT="${SERVICE_ROOT}/systemd/homepi-shairport@.service"
UPSTREAM_REPO="https://github.com/mikebrady/shairport-sync.git"
UPSTREAM_VERSION="4.3.6"

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash ${SERVICE_ROOT}/scripts/install.sh" >&2
    exit 1
  fi
}

ensure_build_deps() {
  local missing=()
  for cmd in git autoconf automake libtool pkg-config make g++ cmake xxd; do
    command -v "${cmd}" >/dev/null 2>&1 || missing+=("${cmd}")
  done
  for pkg in libasound2-dev libmosquitto-dev; do
    case "${pkg}" in
      libasound2-dev) pkg-config --exists alsa 2>/dev/null || missing+=("${pkg}") ;;
      libmosquitto-dev) pkg-config --exists libmosquitto 2>/dev/null || missing+=("${pkg}") ;;
    esac
  done
  if [[ ${#missing[@]} -gt 0 ]]; then
    log "Installing build dependencies"
    apt-get update -qq
    apt-get install -y --no-install-recommends git autoconf automake libtool pkg-config \
      build-essential cmake g++ sqlite3 netcat-openbsd \
      libpopt-dev libconfig-dev libasound2-dev avahi-daemon libavahi-client-dev libssl-dev \
      libsoxr-dev libplist-dev libsodium-dev libavutil-dev libavcodec-dev libavformat-dev \
      uuid-dev libgcrypt20-dev libmosquitto-dev xxd
  fi
}

fetch_upstream() {
  log "Fetching shairport-sync ${UPSTREAM_VERSION}"
  rm -rf "${SRC_DIR}"
  mkdir -p "${BUILD_DIR}"
  git clone --depth 1 --branch "${UPSTREAM_VERSION}" "${UPSTREAM_REPO}" "${SRC_DIR}"
  log "Patching shairport-sync MQTT client id"
  python3 - "${SRC_DIR}/mqtt.c" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
text = path.read_text()
old = "  if (!(global_mosq = mosquitto_new(config.service_name, true, NULL))) {"
new = (
    "  char mqtt_client_id[128];\n"
    "  if (config.mqtt_topic != NULL) {\n"
    "    snprintf(mqtt_client_id, sizeof(mqtt_client_id), \"%s\", config.mqtt_topic);\n"
    "    for (char *p = mqtt_client_id; *p; ++p) {\n"
    "      if (*p == '/') {\n"
    "        *p = '-';\n"
    "      }\n"
    "    }\n"
    "  } else {\n"
    "    snprintf(mqtt_client_id, sizeof(mqtt_client_id), \"%s\", config.service_name);\n"
    "  }\n"
    "  if (!(global_mosq = mosquitto_new(mqtt_client_id, true, NULL))) {"
)
if old not in text:
    raise SystemExit("mqtt client id patch anchor not found")
path.write_text(text.replace(old, new, 1))
PY
  log "Patching shairport-sync to preserve NQPTP master clock on zone teardown"
  python3 - "${SRC_DIR}/rtsp.c" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
text = path.read_text()
old = "    clear_ptp_clock();\n  }"
new = (
    "    // HomePi: do not clear the shared NQPTP clock when one zone tears down PTP.\n"
    "    // Multi-zone AirPlay on one host shares a single /nqptp master clock; clearing it\n"
    "    // silences all remaining zones (shairport-sync #1547, PR #1887).\n"
    "    // clear_ptp_clock();\n"
    "  }"
)
if old not in text:
    raise SystemExit("clear_ptp_clock patch anchor not found")
path.write_text(text.replace(old, new, 1))
PY
}

build_shairport_sync() {
  log "Building shairport-sync into ${INSTALL_ROOT}"
  cd "${SRC_DIR}"
  autoreconf -fi
  PKG_CONFIG_PATH="/opt/homepi/services/nqptp/lib/pkgconfig:${PKG_CONFIG_PATH:-}" \
    ./configure \
      --prefix="${INSTALL_ROOT}" \
      --with-alsa \
      --with-avahi \
      --with-ssl=openssl \
      --with-soxr \
      --with-metadata \
      --with-mqtt-client \
      --with-airplay-2 \
      --with-pipe
  make -j"$(nproc 2>/dev/null || echo 2)"
  make install
  if [[ ! -x "${INSTALL_ROOT}/bin/shairport-sync" ]]; then
    echo "Build failed: ${INSTALL_ROOT}/bin/shairport-sync not found" >&2
    exit 1
  fi
}

build_supervisor() {
  log "Building ${SERVICE_NAME}"
  cmake -S "${SERVICE_ROOT}" -B "${SUPERVISOR_BUILD}"
  cmake --build "${SUPERVISOR_BUILD}" --parallel "$(nproc 2>/dev/null || echo 2)"
  if [[ ! -x "${SUPERVISOR_BUILD}/homepi-shairport-supervisor" ]]; then
    echo "Supervisor build failed" >&2
    exit 1
  fi
}

install_homepi_files() {
  log "Installing HomePi layout"
  install -d -m 0755 "${INSTALL_ROOT}/bin"
  install -d -m 0755 "${INSTALL_ROOT}/config/zones"
  install -d -m 0755 "${INSTALL_ROOT}/bin/hooks"
  install -d -m 0755 "${INSTALL_ROOT}/env"
  install -d -m 0755 "${INSTALL_ROOT}/storage/migrations"

  install -m 0755 "${SUPERVISOR_BUILD}/homepi-shairport-supervisor" \
    "${INSTALL_ROOT}/bin/homepi-shairport-supervisor"
  install -m 0644 "${SERVICE_ROOT}/config/service-config.json" \
    "${INSTALL_ROOT}/config/service-config.json"
  install -m 0644 "${SERVICE_ROOT}/storage/migrations/003-shairport-sync.sql" \
    "${INSTALL_ROOT}/storage/migrations/003-shairport-sync.sql"
  install -m 0440 "${SERVICE_ROOT}/config/homepi-shairport-sudoers" \
    /etc/sudoers.d/homepi-shairport
  visudo -cf /etc/sudoers.d/homepi-shairport
}

install_systemd() {
  log "Installing systemd units"
  install -m 0644 "${SUPERVISOR_UNIT}" "/etc/systemd/system/${SERVICE_NAME}.service"
  install -m 0644 "${ZONE_UNIT}" "/etc/systemd/system/homepi-shairport@.service"
  systemctl daemon-reload
  systemctl enable "${SERVICE_NAME}.service"
  systemctl restart "${SERVICE_NAME}.service"
}

restart_backend() {
  if systemctl is-enabled homepi-backend >/dev/null 2>&1; then
    log "Restarting homepi-backend"
    systemctl restart homepi-backend
  fi
}

verify_install() {
  log "Verifying installation"
  sleep 2
  if ! systemctl is-active "${SERVICE_NAME}.service" >/dev/null 2>&1; then
    echo "Service not active: ${SERVICE_NAME}" >&2
    journalctl -u "${SERVICE_NAME}.service" -n 30 --no-pager >&2 || true
    exit 1
  fi
  if [[ ! -x "${INSTALL_ROOT}/bin/shairport-sync" ]]; then
    echo "shairport-sync binary missing" >&2
    exit 1
  fi
  echo "  OK  ${SERVICE_NAME} active (supervisor offline until controller sync completes)"
}

main() {
  require_root
  ensure_build_deps
  fetch_upstream
  build_shairport_sync
  build_supervisor
  install_homepi_files
  if id homepi >/dev/null 2>&1; then
    chown -R homepi:homepi /opt/homepi
  fi
  install_systemd
  restart_backend
  verify_install

  echo ""
  echo "homepi-shairport-sync installed (upstream ${UPSTREAM_VERSION})."
  echo "  Supervisor: ${INSTALL_ROOT}/bin/homepi-shairport-supervisor"
  echo "  Shairport:  ${INSTALL_ROOT}/bin/shairport-sync"
  echo "  Logs:       journalctl -u ${SERVICE_NAME}.service -f"
}

main "$@"
