#!/usr/bin/env bash
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_NAME="homepi-hifi-serial"
INSTALL_ROOT="/opt/homepi/services/hifi-serial"
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
  for cmd in cmake g++; do
    command -v "${cmd}" >/dev/null 2>&1 || missing+=("${cmd}")
  done
  if ! pkg-config --exists sqlite3 2>/dev/null; then
    missing+=("libsqlite3-dev")
  fi
  if [[ ${#missing[@]} -gt 0 ]]; then
    log "Installing build dependencies: ${missing[*]}"
    apt-get update -qq
    apt-get install -y cmake g++ pkg-config libsqlite3-dev
  fi
}

build_binary() {
  log "Building ${SERVICE_NAME}"
  mkdir -p "${BUILD_DIR}"
  cmake -S "${SERVICE_ROOT}" -B "${BUILD_DIR}"
  cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 2)"
  if [[ ! -x "${BUILD_DIR}/homepi-hifi-serial" ]]; then
    echo "Build failed" >&2
    exit 1
  fi
}

install_files() {
  log "Installing to ${INSTALL_ROOT}"
  install -d -m 0755 "${INSTALL_ROOT}/bin"
  install -d -m 0755 "${INSTALL_ROOT}/config"
  install -d -m 0755 "${INSTALL_ROOT}/storage/migrations"
  install -d -m 0755 "${INSTALL_ROOT}/env"

  install -m 0755 "${BUILD_DIR}/homepi-hifi-serial" "${INSTALL_ROOT}/bin/homepi-hifi-serial"
  install -m 0644 "${SERVICE_ROOT}/config/service-config.json" "${INSTALL_ROOT}/config/service-config.json"
  install -m 0644 "${SERVICE_ROOT}/storage/migrations/002-hifi-serial.sql" \
    "${INSTALL_ROOT}/storage/migrations/002-hifi-serial.sql"
  install -m 0644 "${SERVICE_ROOT}/storage/migrations/003-shairport-sync.sql" \
    "${INSTALL_ROOT}/storage/migrations/003-shairport-sync.sql"

  install -d -m 0755 "${RUNTIME_ROOT}/state"
  install -d -m 0755 "${RUNTIME_ROOT}/generated"
}

install_systemd() {
  log "Installing systemd unit"
  install -m 0644 "${UNIT_SRC}" "${UNIT_DEST}"
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
  systemctl is-active "${SERVICE_NAME}.service"

  if [[ ! -S /run/homepi/hifi-serial.sock ]]; then
    echo "Socket missing: /run/homepi/hifi-serial.sock" >&2
    journalctl -u "${SERVICE_NAME}.service" -n 30 --no-pager >&2 || true
    exit 1
  fi

  local health
  health=$(printf '%s\n' '{"method":"getHealth","correlationId":"install-verify"}' \
    | timeout 3 nc -U /run/homepi/hifi-serial.sock 2>/dev/null | head -1 || true)
  if [[ "${health}" != *'"ok":true'* ]]; then
    echo "Health check failed via Unix socket" >&2
    echo "${health}" >&2
    exit 1
  fi
  echo "  OK  ${SERVICE_NAME} active, socket healthy"
}

main() {
  require_root
  ensure_build_deps
  build_binary
  install_files
  chown -R homepi:homepi /opt/homepi
  install_systemd
  restart_backend
  verify_install
  echo "${SERVICE_NAME} installed."
}

main "$@"
