#!/usr/bin/env bash
# Uninstalls homepi-nqptp systemd service and /opt install tree.
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_NAME="homepi-nqptp"
INSTALL_ROOT="/opt/homepi/services/nqptp"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash ${SERVICE_ROOT}/scripts/uninstall.sh" >&2
    exit 1
  fi
}

stop_service() {
  if systemctl list-unit-files "${SERVICE_NAME}.service" >/dev/null 2>&1; then
    log "Stopping ${SERVICE_NAME}"
    systemctl stop "${SERVICE_NAME}.service" 2>/dev/null || true
    systemctl disable "${SERVICE_NAME}.service" 2>/dev/null || true
  fi
}

remove_systemd() {
  if [[ -f "${UNIT_DEST}" ]]; then
    log "Removing systemd unit"
    rm -f "${UNIT_DEST}"
    systemctl daemon-reload
  fi
}

remove_install_tree() {
  if [[ -d "${INSTALL_ROOT}" ]]; then
    log "Removing ${INSTALL_ROOT}"
    rm -rf "${INSTALL_ROOT}"
  fi
}

restart_backend() {
  if systemctl is-enabled homepi-backend >/dev/null 2>&1; then
    log "Restarting homepi-backend"
    systemctl restart homepi-backend
  fi
}

main() {
  require_root
  stop_service
  remove_systemd
  remove_install_tree
  restart_backend
  echo "${SERVICE_NAME} uninstalled."
}

main "$@"
