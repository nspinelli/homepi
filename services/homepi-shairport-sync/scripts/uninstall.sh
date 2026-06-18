#!/usr/bin/env bash
# Uninstalls homepi-shairport-sync systemd units and /opt install tree.
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "${SERVICE_ROOT}/../.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"
SERVICE_NAME="homepi-shairport-supervisor"
INSTALL_ROOT="/opt/homepi/services/shairport"

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash ${SERVICE_ROOT}/scripts/uninstall.sh" >&2
    exit 1
  fi
}

stop_services() {
  for zone in $(seq 1 16); do
    systemctl stop "homepi-shairport@${zone}.service" 2>/dev/null || true
    systemctl stop "homepi-metadata@${zone}.service" 2>/dev/null || true
  done
  systemctl stop "${SERVICE_NAME}.service" 2>/dev/null || true
  systemctl disable "${SERVICE_NAME}.service" 2>/dev/null || true
}

remove_systemd() {
  rm -f "/etc/systemd/system/${SERVICE_NAME}.service"
  rm -f /etc/systemd/system/homepi-shairport@.service
  systemctl daemon-reload
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
  stop_services
  remove_systemd
  remove_sudoers_dropin /etc/sudoers.d/homepi-shairport
  remove_install_tree
  restart_backend
  echo "${SERVICE_NAME} uninstalled."
}

main "$@"
