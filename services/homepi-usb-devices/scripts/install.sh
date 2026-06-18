#!/usr/bin/env bash
# Installs homepi-usb-devices: build, /opt layout, systemd, and restarts backend.
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "${SERVICE_ROOT}/../.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"
SERVICE_NAME="homepi-usb-devices"
INSTALL_ROOT="/opt/homepi/services/usb-devices"
RUNTIME_ROOT="/opt/homepi/runtime"
BUILD_DIR="${SERVICE_ROOT}/build"
UNIT_SRC="${SERVICE_ROOT}/systemd/${SERVICE_NAME}.service"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"

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
  ensure_build_deps_skip_if_prereqs cmake g++ pkg-config libudev-dev libsqlite3-dev
}

build_binary() {
  log "Building ${SERVICE_NAME}"
  mkdir -p "${BUILD_DIR}"
  cmake -S "${SERVICE_ROOT}" -B "${BUILD_DIR}"
  cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 2)"
  if [[ ! -x "${BUILD_DIR}/homepi-usb-devices" ]]; then
    echo "Build failed: ${BUILD_DIR}/homepi-usb-devices not found" >&2
    exit 1
  fi
}

install_files() {
  log "Installing to ${INSTALL_ROOT}"
  install -d -m 0755 "${INSTALL_ROOT}/bin"
  install -d -m 0755 "${INSTALL_ROOT}/config"
  install -d -m 0755 "${INSTALL_ROOT}/storage/migrations"
  install -d -m 0755 "${INSTALL_ROOT}/env"
  install -d -m 0755 "${INSTALL_ROOT}/scripts"

  install -m 0755 "${BUILD_DIR}/homepi-usb-devices" "${INSTALL_ROOT}/bin/homepi-usb-devices"
  install -m 0644 "${SERVICE_ROOT}/config/service-config.json" "${INSTALL_ROOT}/config/service-config.json"
  install -m 0644 "${SERVICE_ROOT}/storage/migrations/001-usb-devices.sql" \
    "${INSTALL_ROOT}/storage/migrations/001-usb-devices.sql"
  install -m 0644 "${SERVICE_ROOT}/storage/migrations/002-audio-profiles.sql" \
    "${INSTALL_ROOT}/storage/migrations/002-audio-profiles.sql"

  install_root_owned_scripts "${INSTALL_ROOT}/scripts" \
    "${SERVICE_ROOT}/scripts/deploy-udev-rules.sh" \
    "${SERVICE_ROOT}/scripts/deploy-audio-modprobe.sh" \
    "${SERVICE_ROOT}/scripts/apply-primary-audio-alsa.sh" \
    "${SERVICE_ROOT}/scripts/post-assignment-hook.sh"

  install -d -m 0755 "${RUNTIME_ROOT}/state"
  install -d -m 0755 "${RUNTIME_ROOT}/generated"
  install -d -m 0755 "${RUNTIME_ROOT}/cache"
  install -d -m 0755 "${RUNTIME_ROOT}/config"
}

install_sudoers() {
  log "Installing sudoers for post-assignment hook"
  install_sudoers_dropin "${SERVICE_ROOT}/scripts/post-assignment.sudoers" \
    /etc/sudoers.d/homepi-usb-post-assignment
}

install_systemd() {
  log "Installing systemd unit"
  install -m 0644 "${UNIT_SRC}" "${UNIT_DEST}"
  systemctl daemon-reload
  systemctl enable "${SERVICE_NAME}.service"
  systemctl restart "${SERVICE_NAME}.service"
}

deploy_udev_rules() {
  local rules_src="${RUNTIME_ROOT}/generated/udev/99-homepi-usb-devices.rules"
  if [[ -f "${rules_src}" ]]; then
    log "Installing udev rules for /dev/vHifi"
    install -m 0644 "${rules_src}" /etc/udev/rules.d/99-homepi-usb-devices.rules
    udevadm control --reload-rules
    udevadm trigger --subsystem-match=tty --action=add || true
  else
    log "Skipping udev deploy (no generated rules yet; save serial assignment in UI)"
  fi
}

restart_backend() {
  if systemctl list-unit-files "${SERVICE_NAME}.service" >/dev/null 2>&1; then
    :
  fi
  if systemctl is-enabled homepi-backend >/dev/null 2>&1; then
    log "Restarting homepi-backend to connect to USB socket"
    systemctl restart homepi-backend
  else
    log "homepi-backend not installed; skip backend restart"
  fi
}

verify_install() {
  log "Verifying installation"
  sleep 2
  systemctl is-active "${SERVICE_NAME}.service"

  if [[ ! -S /run/homepi/usb-devices.sock ]]; then
    echo "Socket missing: /run/homepi/usb-devices.sock" >&2
    journalctl -u "${SERVICE_NAME}.service" -n 30 --no-pager >&2 || true
    exit 1
  fi

  local health
  health=$(printf '%s\n' '{"method":"getHealth","correlationId":"install-verify"}' \
    | timeout 3 nc -U /run/homepi/usb-devices.sock 2>/dev/null | head -1 || true)
  if [[ "${health}" != *'"ok":true'* ]]; then
    echo "Health check failed via Unix socket" >&2
    echo "${health}" >&2
    exit 1
  fi
  echo "  OK  ${SERVICE_NAME} active, socket healthy"

  if systemctl is-active homepi-backend >/dev/null 2>&1; then
    local code
    code=$(curl -sf -o /dev/null -w "%{http_code}" http://127.0.0.1:3000/api/usb-devices/health 2>/dev/null || echo "000")
    if [[ "${code}" == "200" ]]; then
      echo "  OK  backend proxy /api/usb-devices/health (${code})"
    else
      code=$(curl -sf -o /dev/null -w "%{http_code}" http://127.0.0.1/api/usb-devices/health 2>/dev/null || echo "000")
      if [[ "${code}" == "200" ]]; then
        echo "  OK  backend proxy via nginx (${code})"
      else
        echo "  WARN backend USB health returned ${code} (UI may show offline until backend restarts)"
      fi
    fi
  fi
}

main() {
  require_root
  ensure_build_deps
  build_binary
  install_files
  chown -R homepi:homepi "${RUNTIME_ROOT}"
  chown -R homepi:homepi "${INSTALL_ROOT}/storage" 2>/dev/null || true
  install_sudoers
  install_systemd
  deploy_udev_rules
  restart_backend
  verify_install

  echo ""
  echo "${SERVICE_NAME} is installed and running."
  echo "  Socket: /run/homepi/usb-devices.sock"
  echo "  Logs:   journalctl -u ${SERVICE_NAME}.service -f"
  echo ""
  echo "Ready to test in the UI:"
  echo "  Settings → Audio Configuration"
  echo "  Status   → USB Devices card"
  echo "  http://homepi.local/settings"
}

main "$@"
