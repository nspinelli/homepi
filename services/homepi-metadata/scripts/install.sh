#!/usr/bin/env bash
# Installs homepi-metadata: build upstream metadata reader, /opt layout, systemd, verify.
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_NAME="homepi-metadata"
INSTALL_ROOT="/opt/homepi/services/metadata"
BUILD_DIR="${SERVICE_ROOT}/build"
SRC_DIR="${BUILD_DIR}/metadata-reader-src"
UNIT_SRC="${SERVICE_ROOT}/systemd/${SERVICE_NAME}@.service"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}@.service"
LEGACY_UNIT="${SERVICE_NAME}.service"
WRAPPER_SRC="${SERVICE_ROOT}/scripts/run-metadata-reader.sh"
UPSTREAM_REPO="https://github.com/mikebrady/shairport-sync-metadata-reader.git"
UPSTREAM_VERSION="1.0.3"

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
  for cmd in git autoconf automake libtool pkg-config make gcc; do
    command -v "${cmd}" >/dev/null 2>&1 || missing+=("${cmd}")
  done
  if [[ ${#missing[@]} -gt 0 ]]; then
    log "Installing build dependencies"
    apt-get update -qq
    apt-get install -y git autoconf automake libtool pkg-config build-essential
  fi
}

fetch_upstream() {
  log "Fetching shairport-sync-metadata-reader (v${UPSTREAM_VERSION})"
  rm -rf "${SRC_DIR}"
  mkdir -p "${BUILD_DIR}"
  git clone --depth 1 "${UPSTREAM_REPO}" "${SRC_DIR}"
  log "Applying HomePi FIFO busy-wait fix"
  python3 - "${SRC_DIR}/shairport-sync-metadata-reader.c" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
text = path.read_text()
anchor = "      fflush(stdout);\n    }\n  }\n  return 0;"
replacement = (
    "      fflush(stdout);\n"
    "    } else {\n"
    "      if (feof(stdin)) {\n"
    "        clearerr(stdin);\n"
    "      }\n"
    "      usleep(100000);\n"
    "    }\n"
    "  }\n"
    "  return 0;"
)
if anchor not in text:
    raise SystemExit("metadata reader busy-wait anchor not found")
path.write_text(text.replace(anchor, replacement, 1))
PY
}

build_metadata_reader() {
  log "Building metadata reader into ${INSTALL_ROOT}"
  cd "${SRC_DIR}"
  autoreconf -i -f
  ./configure --prefix="${INSTALL_ROOT}"
  make -j"$(nproc 2>/dev/null || echo 2)"
  make install
  if [[ ! -x "${INSTALL_ROOT}/bin/shairport-sync-metadata-reader" ]]; then
    echo "Build failed: ${INSTALL_ROOT}/bin/shairport-sync-metadata-reader not found" >&2
    exit 1
  fi
}

install_homepi_files() {
  log "Installing HomePi config, wrapper, and env layout"
  install -d -m 0755 "${INSTALL_ROOT}/config"
  install -d -m 0755 "${INSTALL_ROOT}/env"
  install -m 0644 "${SERVICE_ROOT}/config/service-config.json" \
    "${INSTALL_ROOT}/config/service-config.json"
  install -m 0755 "${WRAPPER_SRC}" "${INSTALL_ROOT}/bin/run-metadata-reader.sh"
  install -m 0755 "${SERVICE_ROOT}/scripts/metadata-zone-handler.py" \
    "${INSTALL_ROOT}/bin/metadata-zone-handler.py"
}

install_systemd() {
  log "Installing systemd template unit"
  if systemctl list-unit-files "${LEGACY_UNIT}" >/dev/null 2>&1; then
    systemctl stop "${LEGACY_UNIT}" 2>/dev/null || true
    systemctl disable "${LEGACY_UNIT}" 2>/dev/null || true
    rm -f "/etc/systemd/system/${LEGACY_UNIT}"
  fi
  install -m 0644 "${UNIT_SRC}" "${UNIT_DEST}"
  systemctl daemon-reload
}

restart_backend() {
  if systemctl is-enabled homepi-backend >/dev/null 2>&1; then
    log "Restarting homepi-backend"
    systemctl restart homepi-backend
  else
    log "homepi-backend not installed; skip backend restart"
  fi
}

verify_install() {
  log "Verifying installation"
  if [[ ! -x "${INSTALL_ROOT}/bin/shairport-sync-metadata-reader" ]]; then
    echo "Binary missing after install" >&2
    exit 1
  fi
  echo "  OK  ${SERVICE_NAME}@.service template installed (instances started by shairport supervisor)"
}

main() {
  require_root
  ensure_build_deps
  fetch_upstream
  build_metadata_reader
  install_homepi_files
  if id homepi >/dev/null 2>&1; then
    chown -R homepi:homepi /opt/homepi
  fi
  install_systemd
  restart_backend
  verify_install

  echo ""
  echo "${SERVICE_NAME} installed (shairport-sync-metadata-reader ${UPSTREAM_VERSION})."
  echo "  Binary:  ${INSTALL_ROOT}/bin/shairport-sync-metadata-reader"
  echo "  Wrapper: ${INSTALL_ROOT}/bin/run-metadata-reader.sh"
  echo "  Logs:    journalctl -u ${SERVICE_NAME}.service -f"
  echo ""
  echo "Configure Shairport Sync metadata pipe per zone:"
  echo "  /tmp/homepi-metadata-zone-<1-16>"
}

main "$@"
