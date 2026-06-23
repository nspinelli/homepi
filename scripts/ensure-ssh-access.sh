#!/usr/bin/env bash
# Ensures SSH remains reachable after reboot: service enabled, socket masked, key perms, backups.
set -euo pipefail

log() { echo "==> $*"; }
die() { echo "ERROR: $*" >&2; exit 1; }

SSH_USER="${HOMEPI_SSH_USER:-homepi}"
SSH_DIR="/home/${SSH_USER}/.ssh"
AUTH_KEYS="${SSH_DIR}/authorized_keys"
BACKUP_DIR="/var/backups/homepi/ssh"

ensure_ssh_service() {
  local unit=""
  if systemctl list-unit-files ssh.service >/dev/null 2>&1; then
    unit="ssh.service"
  elif systemctl list-unit-files sshd.service >/dev/null 2>&1; then
    unit="sshd.service"
  else
    echo "WARN: ssh/sshd unit not found" >&2
    return 0
  fi

  systemctl mask ssh.socket 2>/dev/null || true
  systemctl enable "${unit}"
  systemctl start "${unit}" || true
  log "SSH unit enabled and started: ${unit} (ssh.socket masked)"
}

repair_ssh_permissions() {
  if [[ ! -d "${SSH_DIR}" ]]; then
    install -d -m 700 -o "${SSH_USER}" -g "${SSH_USER}" "${SSH_DIR}"
    log "Created ${SSH_DIR}"
  fi

  chown "${SSH_USER}:${SSH_USER}" "${SSH_DIR}"
  chmod 700 "${SSH_DIR}"

  if [[ -f "${AUTH_KEYS}" ]]; then
    chown "${SSH_USER}:${SSH_USER}" "${AUTH_KEYS}"
    chmod 600 "${AUTH_KEYS}"
    log "Repaired ${AUTH_KEYS} permissions"
  else
    echo "WARN: ${AUTH_KEYS} missing — add your public key before rebooting." >&2
  fi

  if [[ -f "${SSH_DIR}/config" ]]; then
    chown "${SSH_USER}:${SSH_USER}" "${SSH_DIR}/config"
    chmod 600 "${SSH_DIR}/config"
  fi
}

backup_ssh_artifacts() {
  local stamp
  stamp="$(date +%Y%m%d-%H%M%S)"
  mkdir -p "${BACKUP_DIR}"

  if [[ -f "${AUTH_KEYS}" ]]; then
    cp -a "${AUTH_KEYS}" "${BACKUP_DIR}/authorized_keys.${stamp}"
    ln -sfn "${BACKUP_DIR}/authorized_keys.${stamp}" "${BACKUP_DIR}/authorized_keys.latest"
    log "Backed up authorized_keys to ${BACKUP_DIR}/authorized_keys.${stamp}"
  fi

  if [[ -f /etc/ssh/sshd_config ]]; then
    cp -a /etc/ssh/sshd_config "${BACKUP_DIR}/sshd_config.${stamp}"
    ln -sfn "${BACKUP_DIR}/sshd_config.${stamp}" "${BACKUP_DIR}/sshd_config.latest"
    log "Backed up sshd_config to ${BACKUP_DIR}/sshd_config.${stamp}"
  fi
}

print_connect_hints() {
  local ips
  ips="$(hostname -I 2>/dev/null | tr ' ' '\n' | grep -E '^[0-9]+\.' | paste -sd ', ' - || true)"
  echo ""
  echo "Connect via:"
  echo "  ssh ${SSH_USER}@homepi.local"
  if [[ -n "${ips}" ]]; then
    echo "  ssh ${SSH_USER}@<ip>   (current IPs: ${ips})"
  fi
  echo ""
  echo "SSH backups: ${BACKUP_DIR}/"
  echo "Restore keys: sudo cp -a ${BACKUP_DIR}/authorized_keys.latest ${AUTH_KEYS}"
}

main() {
  if [[ "${EUID}" -ne 0 ]]; then
    die "Re-run with sudo: sudo bash scripts/ensure-ssh-access.sh"
  fi
  log "Ensuring SSH access survives reboot"
  ensure_ssh_service
  repair_ssh_permissions
  backup_ssh_artifacts
  print_connect_hints
  log "SSH access hardening complete"
}

main "$@"
