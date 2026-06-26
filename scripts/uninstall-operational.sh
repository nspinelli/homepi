#!/usr/bin/env bash
# Uninstalls operational HomePi stack: backend service, native services, optional NGINX alias.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash ${REPO_ROOT}/scripts/uninstall-operational.sh" >&2
    exit 1
  fi
}

main() {
  require_root

  if systemctl list-unit-files homepi-backend.service >/dev/null 2>&1; then
    log "Stopping homepi-backend"
    systemctl stop homepi-backend.service 2>/dev/null || true
    systemctl disable homepi-backend.service 2>/dev/null || true
  fi
  if [[ -f /etc/systemd/system/homepi-backend.service ]]; then
    log "Removing homepi-backend systemd unit"
    rm -f /etc/systemd/system/homepi-backend.service
    systemctl daemon-reload
  fi

  if systemctl list-unit-files avahi-homepi-alias.service >/dev/null 2>&1; then
    log "Stopping avahi-homepi-alias"
    systemctl stop avahi-homepi-alias.service 2>/dev/null || true
    systemctl disable avahi-homepi-alias.service 2>/dev/null || true
  fi
  if [[ -f /etc/systemd/system/avahi-homepi-alias.service ]]; then
    rm -f /etc/systemd/system/avahi-homepi-alias.service
    systemctl daemon-reload
  fi

  log "Uninstalling native services"
  bash "${REPO_ROOT}/scripts/uninstall-services.sh"

  if [[ -f "${REPO_ROOT}/core/events/scripts/uninstall.sh" ]]; then
    log "Uninstalling core/events broker"
    bash "${REPO_ROOT}/core/events/scripts/uninstall.sh"
  fi

  if [[ -f "${REPO_ROOT}/scripts/uninstall-ensure-ssh.sh" ]]; then
    log "Uninstalling SSH boot hardening"
    bash "${REPO_ROOT}/scripts/uninstall-ensure-ssh.sh"
  fi

  log "Operational uninstall complete (NGINX site left in place; remove manually if needed)"
}

main "$@"
