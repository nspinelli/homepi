#!/usr/bin/env bash
# Installs SSH boot hardening: masks ssh.socket, enables ssh.service, repairs key perms.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

SCRIPT_DEST="/opt/homepi/scripts/ensure-ssh-access.sh"
UNIT_SRC="${REPO_ROOT}/infra/systemd/homepi-ensure-ssh.service"
UNIT_DEST="/etc/systemd/system/homepi-ensure-ssh.service"
SERVICE_NAME="homepi-ensure-ssh"

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    die "Re-run with sudo: sudo bash ${REPO_ROOT}/scripts/install-ensure-ssh.sh"
  fi
}

main() {
  require_root
  log "Installing ${SERVICE_NAME}"

  install -d -m 0755 /opt/homepi/scripts
  install -m 0755 "${REPO_ROOT}/scripts/ensure-ssh-access.sh" "${SCRIPT_DEST}"
  install -m 0644 "${UNIT_SRC}" "${UNIT_DEST}"

  systemctl daemon-reload
  systemctl enable "${SERVICE_NAME}.service"
  systemctl start "${SERVICE_NAME}.service"

  log "${SERVICE_NAME} installed and started"
}

main "$@"
