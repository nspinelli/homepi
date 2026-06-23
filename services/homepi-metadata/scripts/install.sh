#!/usr/bin/env bash
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "${SERVICE_ROOT}/../.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"
SERVICE_NAME="homepi-metadata"
INSTALL_ROOT="/opt/homepi/services/metadata"
RUNTIME_ROOT="/opt/homepi/runtime"
BUILD_DIR="${SERVICE_ROOT}/build"
UNIT_SRC="${SERVICE_ROOT}/systemd/${SERVICE_NAME}.service"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"
LEGACY_TEMPLATE_UNIT="/etc/systemd/system/homepi-metadata@.service"

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
  cmake -S "${SERVICE_ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
  cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 2)"
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -R "test_metadata_parser|test_progress_parser"
}

install_files() {
  log "Installing to ${INSTALL_ROOT}"
  install -d -m 0755 "${INSTALL_ROOT}/bin"
  install -d -m 0755 "${INSTALL_ROOT}/config"
  install -d -m 0755 "${INSTALL_ROOT}/env"
  install -d -m 0755 "${RUNTIME_ROOT}/state"
  install -d -m 0755 "${RUNTIME_ROOT}/cache"
  install -d -m 0755 /run/homepi

  install -m 0755 "${BUILD_DIR}/homepi-metadata" "${INSTALL_ROOT}/bin/homepi-metadata"
  install -m 0644 "${SERVICE_ROOT}/config/service-config.json" "${INSTALL_ROOT}/config/service-config.json"
  install -m 0644 "${SERVICE_ROOT}/config/homepi-metadata.env.example" \
    "${INSTALL_ROOT}/env/.env.example"
  if [[ ! -f "${INSTALL_ROOT}/env/.env" ]]; then
    install -m 0644 "${SERVICE_ROOT}/config/homepi-metadata.env.example" \
      "${INSTALL_ROOT}/env/.env"
  fi
}

disable_legacy_units() {
  log "Disabling legacy per-zone metadata units"
  systemctl stop "homepi-metadata@".service 2>/dev/null || true
  for unit in /etc/systemd/system/homepi-metadata@*.service; do
    [[ -e "${unit}" ]] || continue
    systemctl disable "$(basename "${unit}")" 2>/dev/null || true
  done
  if [[ -f "${LEGACY_TEMPLATE_UNIT}" ]]; then
    systemctl disable homepi-metadata@.service 2>/dev/null || true
    rm -f "${LEGACY_TEMPLATE_UNIT}"
  fi
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
  disable_legacy_units
  install_systemd
  log "${SERVICE_NAME} installed and started"
}

main "$@"
