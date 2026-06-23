#!/usr/bin/env bash
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "${SERVICE_ROOT}/../.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"
SERVICE_NAME="homepi-audio-orchestrator"
INSTALL_ROOT="/opt/homepi/services/audio-orchestrator"
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
  ensure_build_deps_skip_if_prereqs cmake g++ pkg-config libsqlite3-dev
}

build_binary() {
  log "Building ${SERVICE_NAME}"
  mkdir -p "${BUILD_DIR}"
  cmake -S "${SERVICE_ROOT}" -B "${BUILD_DIR}"
  cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 2)"
  if [[ ! -x "${BUILD_DIR}/homepi-audio-orchestrator" ]]; then
    echo "Build failed" >&2
    exit 1
  fi
}

install_files() {
  log "Installing to ${INSTALL_ROOT}"
  install -d -m 0755 "${INSTALL_ROOT}/bin"
  install -d -m 0755 "${INSTALL_ROOT}/config"
  install -d -m 0755 "${INSTALL_ROOT}/env"

  install -m 0755 "${BUILD_DIR}/homepi-audio-orchestrator" "${INSTALL_ROOT}/bin/homepi-audio-orchestrator"
  install -m 0644 "${SERVICE_ROOT}/config/service-config.json" "${INSTALL_ROOT}/config/service-config.json"
}

install_systemd() {
  log "Installing systemd unit"
  install -m 0644 "${UNIT_SRC}" "${UNIT_DEST}"
  systemctl daemon-reload
  systemctl enable "${SERVICE_NAME}.service"
  systemctl restart "${SERVICE_NAME}.service"
}

verify_install() {
  log "Verifying installation"
  sleep 2
  if ! systemctl is-active "${SERVICE_NAME}.service" >/dev/null 2>&1; then
    echo "Service not active: ${SERVICE_NAME}" >&2
    journalctl -u "${SERVICE_NAME}.service" -n 30 --no-pager >&2 || true
    exit 1
  fi
  echo "  OK  ${SERVICE_NAME} active"
}

main() {
  require_root
  ensure_build_deps
  build_binary
  install_files
  install_systemd
  verify_install
  echo "${SERVICE_NAME} installed."
}

main "$@"
