#!/usr/bin/env bash
# Installs homepi-nqptp: build upstream nqptp, /opt layout, systemd, verify.
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "${SERVICE_ROOT}/../.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

SERVICE_NAME="homepi-nqptp"
INSTALL_ROOT="/opt/homepi/services/nqptp"
BUILD_DIR="${SERVICE_ROOT}/build"
SRC_DIR="${BUILD_DIR}/nqptp-src"
UNIT_SRC="${SERVICE_ROOT}/systemd/${SERVICE_NAME}.service"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"
CONFIG_JSON="${SERVICE_ROOT}/config/service-config.json"
PATCH_FILE="${SERVICE_ROOT}/patches/multi-zone-play-client-refcount.patch"
UPSTREAM_REPO="https://github.com/mikebrady/nqptp.git"
LEGACY_UNIT="nqptp.service"

read_upstream_version() {
  UPSTREAM_VERSION="$(read_json_field "${CONFIG_JSON}" "nqptp.upstreamVersion")" \
    || die "Could not read nqptp.upstreamVersion from ${CONFIG_JSON}"
}

ensure_build_deps() {
  ensure_build_deps_skip_if_prereqs git autoconf automake libtool pkg-config build-essential
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
  log "Validating multi-zone patch for nqptp ${UPSTREAM_VERSION}"
  apply_patch "${PATCH_FILE}" "${SRC_DIR}"
}

build_nqptp() {
  log "Building nqptp into ${INSTALL_ROOT}"
  cd "${SRC_DIR}"
  autoreconf -fi
  ./configure --prefix="${INSTALL_ROOT}"
  make -j"$(nproc 2>/dev/null || echo 2)"
  make install
  if [[ ! -x "${INSTALL_ROOT}/bin/nqptp" ]]; then
    die "Build failed: ${INSTALL_ROOT}/bin/nqptp not found"
  fi
}

install_homepi_files() {
  log "Installing HomePi config and env layout"
  install -d -m 0755 "${INSTALL_ROOT}/config"
  install -d -m 0755 "${INSTALL_ROOT}/env"
  install -m 0644 "${CONFIG_JSON}" "${INSTALL_ROOT}/config/service-config.json"
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
    die "Unexpected nqptp version output: ${version_out}"
  fi
  echo "  OK  ${SERVICE_NAME} active (${version_out%%$'\n'*})"
}

main() {
  require_root
  read_upstream_version
  ensure_build_deps
  remove_legacy_unit
  fetch_upstream
  build_nqptp
  install_homepi_files
  chown_homepi_runtime
  install_systemd
  restart_backend
  verify_install

  echo ""
  echo "${SERVICE_NAME} installed (upstream nqptp ${UPSTREAM_VERSION})."
  echo "  Binary: ${INSTALL_ROOT}/bin/nqptp"
  echo "  Logs:   journalctl -u ${SERVICE_NAME}.service -f"
}

main "$@"
