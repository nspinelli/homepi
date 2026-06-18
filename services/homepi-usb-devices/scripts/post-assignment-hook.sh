#!/usr/bin/env bash
# Deploys udev/modprobe artifacts and restarts dependent services after assignment save.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOCK_FILE="/run/homepi/post-assignment-hook.lock"
SERIAL_CHANGED="${1:-${SERIAL_CHANGED:-1}}"
AUDIO_CHANGED="${2:-${AUDIO_CHANGED:-1}}"
HOMEPI_ALLOW_REBOOT="${HOMEPI_ALLOW_REBOOT:-1}"

log() { echo "==> $*"; }

schedule_reboot() {
  local reason="$1"
  mkdir -p /run/homepi
  echo "${reason}" > /run/homepi/pending-reboot-reason
  log "Reboot required: ${reason}"

  if [[ "${HOMEPI_ALLOW_REBOOT}" != "1" ]]; then
    echo "Reboot blocked (HOMEPI_ALLOW_REBOOT=0). Apply ALSA changes manually or set HOMEPI_ALLOW_REBOOT=1." >&2
    exit 1
  fi

  local delay="${HOMEPI_REBOOT_DELAY_SEC:-0}"
  if [[ "${delay}" -gt 0 ]]; then
    log "Rebooting in ${delay}s (HOMEPI_REBOOT_DELAY_SEC)"
    sleep "${delay}"
  fi

  sync
  systemctl reboot --no-wall
  exit 0
}

exec 9>"${LOCK_FILE}"
if ! flock -n 9; then
  log "Another post-assignment hook is running; waiting"
  flock 9
fi

wait_for_service() {
  local unit="$1"
  local socket_path="$2"
  local timeout="${3:-30}"

  for _ in $(seq 1 "${timeout}"); do
    if systemctl is-active --quiet "${unit}" && [[ -S "${socket_path}" ]]; then
      if echo '{"method":"getHealth","correlationId":"post-assignment-wait"}' \
        | nc -U -w 1 "${socket_path}" >/dev/null 2>&1; then
        return 0
      fi
    fi
    sleep 1
  done
  log "WARN: ${unit} did not become ready within ${timeout}s"
  return 1
}

restart_service() {
  local unit="$1"
  local socket_path="$2"
  if ! systemctl is-enabled "${unit}" >/dev/null 2>&1; then
    return 0
  fi
  log "Restarting ${unit}"
  systemctl restart "${unit}"
  wait_for_service "${unit}" "${socket_path}" 30 || true
}

PRIMARY_MODPROBE_CHANGED=false

if [[ "${AUDIO_CHANGED}" == "1" ]]; then
  deploy_rc=0
  if bash "${SCRIPT_DIR}/deploy-audio-modprobe.sh"; then
    deploy_rc=0
  else
    deploy_rc=$?
  fi

  if [[ "${deploy_rc}" -eq 0 ]]; then
    PRIMARY_MODPROBE_CHANGED=true
    log "Primary audio modprobe changed — applying stable ALSA name"
    if bash "${SCRIPT_DIR}/apply-primary-audio-alsa.sh"; then
      log "HomePiPrimary ALSA name is active"
    else
      schedule_reboot "HomePiPrimary ALSA name requires reboot after primary audio modprobe change"
    fi
  elif [[ "${deploy_rc}" -ne 2 ]]; then
    log "WARN: primary audio modprobe deploy failed (check Primary Audio Output assignment)"
  fi
fi

if [[ "${SERIAL_CHANGED}" == "1" ]]; then
  if ! bash "${SCRIPT_DIR}/deploy-udev-rules.sh"; then
    log "WARN: serial udev deploy skipped (assign Primary Serial or check /dev/vHifi)"
  fi
  restart_service "homepi-hifi-serial.service" "/run/homepi/hifi-serial.sock"
fi

if [[ "${AUDIO_CHANGED}" == "1" ]]; then
  restart_service "homepi-pcm-router.service" "/run/homepi/pcm-router.sock"
fi

log "Post-assignment hook complete"
