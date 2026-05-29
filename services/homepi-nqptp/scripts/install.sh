#!/usr/bin/env bash
# Installs homepi-nqptp: build upstream nqptp, /opt layout, systemd, verify.
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_NAME="homepi-nqptp"
INSTALL_ROOT="/opt/homepi/services/nqptp"
BUILD_DIR="${SERVICE_ROOT}/build"
SRC_DIR="${BUILD_DIR}/nqptp-src"
UNIT_SRC="${SERVICE_ROOT}/systemd/${SERVICE_NAME}.service"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"
UPSTREAM_REPO="https://github.com/mikebrady/nqptp.git"
UPSTREAM_VERSION="1.2.8"
LEGACY_UNIT="nqptp.service"

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

remove_legacy_unit() {
  if systemctl list-unit-files "${LEGACY_UNIT}" >/dev/null 2>&1; then
    log "Removing legacy upstream unit ${LEGACY_UNIT}"
    systemctl stop "${LEGACY_UNIT}" 2>/dev/null || true
    systemctl disable "${LEGACY_UNIT}" 2>/dev/null || true
  fi
  for path in /lib/systemd/system/nqptp.service /usr/local/lib/systemd/system/nqptp.service \
    /etc/systemd/system/nqptp.service; do
    if [[ -f "${path}" ]]; then
      log "Removing ${path}"
      rm -f "${path}"
    fi
  done
  systemctl daemon-reload 2>/dev/null || true
}

fetch_upstream() {
  log "Fetching nqptp ${UPSTREAM_VERSION}"
  rm -rf "${SRC_DIR}"
  mkdir -p "${BUILD_DIR}"
  git clone --depth 1 --branch "${UPSTREAM_VERSION}" "${UPSTREAM_REPO}" "${SRC_DIR}"
}

build_nqptp() {
  log "Building nqptp into ${INSTALL_ROOT}"
  cd "${SRC_DIR}"
  autoreconf -fi
  ./configure --prefix="${INSTALL_ROOT}"
  make -j"$(nproc 2>/dev/null || echo 2)"
  make install
  if [[ ! -x "${INSTALL_ROOT}/bin/nqptp" ]]; then
    echo "Build failed: ${INSTALL_ROOT}/bin/nqptp not found" >&2
    exit 1
  fi
}

install_homepi_files() {
  log "Installing HomePi config and env layout"
  install -d -m 0755 "${INSTALL_ROOT}/config"
  install -d -m 0755 "${INSTALL_ROOT}/env"
  install -m 0644 "${SERVICE_ROOT}/config/service-config.json" \
    "${INSTALL_ROOT}/config/service-config.json"
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

  local version_out
  version_out="$("${INSTALL_ROOT}/bin/nqptp" -V 2>&1 || true)"
  if [[ "${version_out}" != *"${UPSTREAM_VERSION}"* ]] && [[ "${version_out}" != *"1.2"* ]]; then
    echo "Unexpected nqptp version output: ${version_out}" >&2
    exit 1
  fi
  echo "  OK  ${SERVICE_NAME} active (${version_out%%$'\n'*})"
}

main() {
  require_root
  ensure_build_deps
  remove_legacy_unit
  fetch_upstream
  build_nqptp
  install_homepi_files
  if id homepi >/dev/null 2>&1; then
    chown -R homepi:homepi /opt/homepi
  fi
  install_systemd
  restart_backend
  verify_install

  echo ""
  echo "${SERVICE_NAME} installed (upstream nqptp ${UPSTREAM_VERSION})."
  echo "  Binary: ${INSTALL_ROOT}/bin/nqptp"
  echo "  Logs:   journalctl -u ${SERVICE_NAME}.service -f"
}

main "$@"
