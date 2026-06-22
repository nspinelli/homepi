#!/usr/bin/env bash
# Uninstalls core/events broker systemd unit and /opt layout.
set -euo pipefail

CORE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_NAME="homepi-events"
INSTALL_ROOT="/opt/homepi/services/events"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash ${CORE_ROOT}/scripts/uninstall.sh" >&2
    exit 1
  fi
}

main() {
  require_root
  log "Stopping ${SERVICE_NAME}"
  systemctl stop "${SERVICE_NAME}.service" 2>/dev/null || true
  systemctl disable "${SERVICE_NAME}.service" 2>/dev/null || true
  rm -f "${UNIT_DEST}"
  systemctl daemon-reload
  rm -rf "${INSTALL_ROOT}"
  rm -f /run/homepi/events.sock
  log "${SERVICE_NAME} uninstalled"
}

main "$@"
