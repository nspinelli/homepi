#!/usr/bin/env bash
# Installs homepi-metadata: build upstream metadata reader, /opt layout, systemd, verify.
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_NAME="homepi-metadata"
INSTALL_ROOT="/opt/homepi/services/metadata"
BUILD_DIR="${SERVICE_ROOT}/build"
SRC_DIR="${BUILD_DIR}/metadata-reader-src"
UNIT_SRC="${SERVICE_ROOT}/systemd/${SERVICE_NAME}.service"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"
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
  else
    log "homepi-backend not installed; skip backend restart"
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
  if [[ ! -x "${INSTALL_ROOT}/bin/shairport-sync-metadata-reader" ]]; then
    echo "Binary missing after install" >&2
    exit 1
  fi
  echo "  OK  ${SERVICE_NAME} active (upstream ${UPSTREAM_VERSION})"
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
  echo "Configure Shairport Sync metadata pipe to match:"
  echo "  /tmp/shairport-sync-metadata (or HOMEPPI_METADATA_PIPE in env/.env)"
}

main "$@"
