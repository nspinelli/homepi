#!/usr/bin/env bash
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_NAME="homepi-pcm-router"
INSTALL_ROOT="/opt/homepi/services/pcm-router"
RUNTIME_ROOT="/opt/homepi/runtime"
BUILD_DIR="${SERVICE_ROOT}/build"
UNIT_SRC="${SERVICE_ROOT}/systemd/${SERVICE_NAME}.service"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"

log() { echo "==> $*"; }

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash ${SERVICE_ROOT}/scripts/install.sh" >&2
    exit 1
  fi
}

ensure_build_deps() {
  local missing=()
  for cmd in cmake gcc pkg-config; do
    command -v "${cmd}" >/dev/null 2>&1 || missing+=("${cmd}")
  done
  for pkg in alsa libmosquitto sqlite3; do
    if ! pkg-config --exists "${pkg}" 2>/dev/null; then
      case "${pkg}" in
        alsa) missing+=("libasound2-dev") ;;
        libmosquitto) missing+=("libmosquitto-dev") ;;
        sqlite3) missing+=("libsqlite3-dev") ;;
      esac
    fi
  done
  if [[ ${#missing[@]} -gt 0 ]]; then
    log "Installing build dependencies: ${missing[*]}"
    apt-get update -qq
    apt-get install -y cmake gcc pkg-config libasound2-dev libmosquitto-dev libsqlite3-dev \
      alsa-utils mosquitto mosquitto-clients
  fi
}

ensure_mosquitto() {
  if ! systemctl list-unit-files mosquitto.service >/dev/null 2>&1; then
    log "Installing mosquitto broker"
    apt-get update -qq
    apt-get install -y mosquitto mosquitto-clients
  fi
  if ! systemctl is-enabled mosquitto.service >/dev/null 2>&1; then
    log "Enabling mosquitto"
    systemctl enable mosquitto.service
    systemctl start mosquitto.service
  fi
}

install_alsa_loopback() {
  log "Installing ALSA loopback module configuration"
  install -m 0644 "${SERVICE_ROOT}/config/homepi-aloop-modules-load.conf" \
    /etc/modules-load.d/homepi-aloop.conf
  install -m 0644 "${SERVICE_ROOT}/config/homepi-aloop-modprobe.conf" \
    /etc/modprobe.d/homepi-aloop.conf

  if lsmod | grep -q snd_aloop; then
    log "snd-aloop already loaded; reboot if cards are missing"
  else
    modprobe snd-aloop || true
  fi
}

build_binary() {
  log "Building ${SERVICE_NAME}"
  mkdir -p "${BUILD_DIR}"
  cmake -S "${SERVICE_ROOT}" -B "${BUILD_DIR}"
  cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 2)"
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
}

install_files() {
  log "Installing to ${INSTALL_ROOT}"
  install -d -m 0755 "${INSTALL_ROOT}/bin"
  install -d -m 0755 "${INSTALL_ROOT}/config"
  install -d -m 0755 "${INSTALL_ROOT}/env"

  install -m 0755 "${BUILD_DIR}/homepi-pcm-router" "${INSTALL_ROOT}/bin/homepi-pcm-router"
  install -m 0644 "${SERVICE_ROOT}/config/homepi-pcm-router.env.example" \
    "${INSTALL_ROOT}/env/.env.example"
  if [[ ! -f "${INSTALL_ROOT}/env/.env" ]]; then
    install -m 0644 "${SERVICE_ROOT}/config/homepi-pcm-router.env.example" \
      "${INSTALL_ROOT}/env/.env"
  fi
  install -d -m 0755 "${RUNTIME_ROOT}/state"
}

validate_alsa() {
  log "Validating ALSA loopback substreams"
  set -a
  # shellcheck source=/dev/null
  source "${INSTALL_ROOT}/env/.env"
  set +a
  "${INSTALL_ROOT}/bin/homepi-pcm-router" --validate-alsa
}

install_systemd() {
  log "Installing systemd unit"
  install -m 0644 "${UNIT_SRC}" "${UNIT_DEST}"
  systemctl daemon-reload
  systemctl enable "${SERVICE_NAME}.service"
}

try_start_service() {
  log "Starting ${SERVICE_NAME} (degraded mode OK when DAC is not ready)"
  systemctl restart "${SERVICE_NAME}.service" || true
}

restart_backend() {
  if systemctl is-enabled homepi-backend >/dev/null 2>&1; then
    log "Restarting homepi-backend"
    systemctl restart homepi-backend
  fi
}

main() {
  require_root
  ensure_build_deps
  ensure_mosquitto
  install_alsa_loopback
  build_binary
  install_files
  validate_alsa
  install_systemd
  try_start_service
  restart_backend
  log "Installation complete"
}

main "$@"
