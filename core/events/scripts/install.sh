#!/usr/bin/env bash
# Installs core/events broker: build, /opt layout, systemd.
set -euo pipefail

CORE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "${CORE_ROOT}/../.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"
SERVICE_NAME="homepi-events"
INSTALL_ROOT="/opt/homepi/services/events"
BUILD_DIR="${CORE_ROOT}/cpp/build"
UNIT_SRC="${CORE_ROOT}/systemd/${SERVICE_NAME}.service"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash ${CORE_ROOT}/scripts/install.sh" >&2
    exit 1
  fi
}

ensure_build_deps() {
  ensure_build_deps_skip_if_prereqs cmake g++
}

build_binary() {
  log "Building ${SERVICE_NAME}"
  mkdir -p "${BUILD_DIR}"
  cmake -S "${CORE_ROOT}/cpp" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
  cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 2)"
  if [[ ! -x "${BUILD_DIR}/homepi-events" ]]; then
    echo "Build failed: ${BUILD_DIR}/homepi-events not found" >&2
    exit 1
  fi
  (cd "${BUILD_DIR}" && ctest --output-on-failure)
}

install_files() {
  log "Installing to ${INSTALL_ROOT}"
  install -d -m 0755 "${INSTALL_ROOT}/bin"
  install -m 0755 "${BUILD_DIR}/homepi-events" "${INSTALL_ROOT}/bin/homepi-events"
}

install_systemd() {
  log "Installing systemd unit"
  install -m 0644 "${UNIT_SRC}" "${UNIT_DEST}"
  systemctl daemon-reload
  systemctl enable "${SERVICE_NAME}.service"
  systemctl restart "${SERVICE_NAME}.service"
}

main() {
  require_root
  ensure_build_deps
  build_binary
  install_files
  install_systemd
  log "${SERVICE_NAME} installed"
}

main "$@"
