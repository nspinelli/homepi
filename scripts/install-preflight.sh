#!/usr/bin/env bash
# Pre-install checks: backups, SSH sanity, ALSA snapshot. Does not mutate system config.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

REPAIR_SSH_PERMS=0

usage() {
  cat <<'EOF'
Usage: bash scripts/install-preflight.sh [--repair-ssh-perms]

Runs before install-operational.sh:
  - Warns if no active SSH session
  - Backs up sshd, sudoers, and HomePi modprobe configs
  - Verifies sshd is enabled
  - Checks homepi ~/.ssh permissions (repair only with --repair-ssh-perms)
EOF
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --repair-ssh-perms) REPAIR_SSH_PERMS=1; shift ;;
      -h|--help) usage; exit 0 ;;
      *) die "Unknown option: $1" ;;
    esac
  done
}

warn_ssh_session() {
  if [[ -z "${SSH_CONNECTION:-}" ]]; then
    echo "WARN: No active SSH session detected. Use serial console or physical access as fallback." >&2
  else
    log "Active SSH session: ${SSH_CONNECTION}"
  fi
}

backup_critical_files() {
  init_backup_session
  backup_system_file /etc/ssh/sshd_config
  backup_system_file /etc/sudoers
  local f
  shopt -s nullglob
  for f in /etc/sudoers.d/*; do
    backup_system_file "${f}"
  done
  for f in /etc/modprobe.d/homepi-*.conf; do
    backup_system_file "${f}"
  done
  for f in /etc/modules-load.d/homepi-*.conf; do
    backup_system_file "${f}"
  done
  shopt -u nullglob

  local cards_dest="${HOMEPI_BACKUP_ROOT}/install-${HOMEPI_BACKUP_SESSION}/proc-asound-cards.txt"
  if [[ -r /proc/asound/cards ]]; then
    cp /proc/asound/cards "${cards_dest}"
    log "Saved ALSA snapshot to ${cards_dest}"
  fi
}

assert_sshd_enabled() {
  local unit=""
  if systemctl list-unit-files ssh.service >/dev/null 2>&1; then
    unit="ssh.service"
  elif systemctl list-unit-files sshd.service >/dev/null 2>&1; then
    unit="sshd.service"
  else
    echo "WARN: ssh/sshd unit not found; ensure SSH is available before continuing." >&2
    return 0
  fi
  if ! systemctl is-enabled "${unit}" >/dev/null 2>&1; then
    die "SSH is not enabled (${unit}). Enable it before install: sudo systemctl enable ${unit}"
  fi
  log "SSH unit enabled: ${unit}"
}

check_ssh_permissions() {
  if ! id homepi >/dev/null 2>&1; then
    log "User homepi not found; skipping SSH key permission check"
    return 0
  fi
  local ssh_dir="/home/homepi/.ssh"
  local auth_keys="${ssh_dir}/authorized_keys"
  if [[ ! -d "${ssh_dir}" ]]; then
    log "No ${ssh_dir}; skipping permission check"
    return 0
  fi

  local dir_perm auth_perm
  dir_perm="$(stat -c '%a' "${ssh_dir}" 2>/dev/null || echo "")"
  if [[ "${dir_perm}" != "700" ]]; then
    if [[ "${REPAIR_SSH_PERMS}" -eq 1 ]]; then
      chmod 700 "${ssh_dir}"
      chown homepi:homepi "${ssh_dir}"
      log "Repaired ${ssh_dir} permissions to 700"
    else
      echo "WARN: ${ssh_dir} permissions are ${dir_perm} (expected 700). Use --repair-ssh-perms to fix." >&2
    fi
  fi

  if [[ -f "${auth_keys}" ]]; then
    auth_perm="$(stat -c '%a' "${auth_keys}" 2>/dev/null || echo "")"
    if [[ "${auth_perm}" != "600" ]]; then
      if [[ "${REPAIR_SSH_PERMS}" -eq 1 ]]; then
        chmod 600 "${auth_keys}"
        chown homepi:homepi "${auth_keys}"
        log "Repaired ${auth_keys} permissions to 600"
      else
        echo "WARN: ${auth_keys} permissions are ${auth_perm} (expected 600). Use --repair-ssh-perms to fix." >&2
      fi
    fi
  fi
}

main() {
  parse_args "$@"
  if [[ "${EUID}" -ne 0 ]]; then
    die "Re-run with sudo: sudo bash scripts/install-preflight.sh"
  fi
  log "HomePi install preflight"
  warn_ssh_session
  backup_critical_files
  assert_sshd_enabled
  check_ssh_permissions
  echo ""
  echo "Preflight complete. Backups: ${HOMEPI_BACKUP_ROOT}/install-${HOMEPI_BACKUP_SESSION}"
  echo "Next: sudo bash scripts/install-operational.sh"
}

main "$@"
