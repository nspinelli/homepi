#!/usr/bin/env bash
# Removes HomePi SSH boot hardening unit (does not unmask ssh.socket).
set -euo pipefail

SERVICE_NAME="homepi-ensure-ssh"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"
SCRIPT_DEST="/opt/homepi/scripts/ensure-ssh-access.sh"

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash scripts/uninstall-ensure-ssh.sh" >&2
    exit 1
  fi
}

main() {
  require_root
  log "Uninstalling ${SERVICE_NAME}"

  systemctl stop "${SERVICE_NAME}.service" 2>/dev/null || true
  systemctl disable "${SERVICE_NAME}.service" 2>/dev/null || true
  rm -f "${UNIT_DEST}"
  systemctl daemon-reload

  if [[ -f "${SCRIPT_DEST}" ]]; then
    rm -f "${SCRIPT_DEST}"
  fi
  rmdir /opt/homepi/scripts 2>/dev/null || true

  log "${SERVICE_NAME} uninstalled (ssh.socket left as-is; ssh.service remains enabled)"
}

main "$@"
