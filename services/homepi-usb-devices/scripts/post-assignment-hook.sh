#!/usr/bin/env bash
# Deploys udev/modprobe artifacts and restarts dependent services after assignment save.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOCK_FILE="/run/homepi/post-assignment-hook.lock"
SERIAL_CHANGED="${1:-${SERIAL_CHANGED:-1}}"
AUDIO_CHANGED="${2:-${AUDIO_CHANGED:-1}}"
HOMEPI_ALLOW_REBOOT="${HOMEPI_ALLOW_REBOOT:-0}"
AUDIO_SERVICES_FINALIZED=0

log() { echo "==> $*"; }

rebind_stranded_usb_ports() {
  local dev port
  for dev in /sys/bus/usb/devices/[0-9]*-[0-9]*; do
    [[ -f "${dev}/idVendor" ]] || continue
    [[ -e "${dev}/driver" ]] && continue
    port=$(basename "${dev}")
    log "Re-binding stranded USB port ${port}"
    echo "${port}" > /sys/bus/usb/drivers/usb/bind 2>/dev/null || true
  done
}

restart_audio_dependent_services() {
  if [[ "${AUDIO_CHANGED}" != "1" ]]; then
    return 0
  fi

  restart_service "homepi-pcm-router.service" "/run/homepi/audio/pcm-router.sock" || true
  if systemctl is-enabled homepi-shairport-supervisor.service >/dev/null 2>&1; then
    log "Restarting homepi-shairport-supervisor.service"
    systemctl restart homepi-shairport-supervisor.service || true
  fi
  modprobe snd-usb-audio 2>/dev/null || true
  rebind_stranded_usb_ports
}

finalize_hook() {
  local rc=$?
  if [[ "${AUDIO_SERVICES_FINALIZED}" != "1" ]]; then
    restart_audio_dependent_services || true
  fi
  exit "${rc}"
}

schedule_reboot() {
  local reason="$1"
  mkdir -p /run/homepi
  echo "${reason}" > /run/homepi/pending-reboot-reason
  log "Reboot required: ${reason}"

  if [[ "${HOMEPI_ALLOW_REBOOT}" != "1" ]]; then
    echo "Reboot blocked (HOMEPI_ALLOW_REBOOT=0). Apply ALSA changes manually or set HOMEPI_ALLOW_REBOOT=1." >&2
    return 1
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

trap finalize_hook EXIT

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

if [[ "${SERIAL_CHANGED}" == "1" ]]; then
  if ! bash "${SCRIPT_DIR}/deploy-udev-rules.sh"; then
    log "WARN: serial udev deploy skipped (assign Primary Serial or check /dev/vHifi)"
  fi
  restart_service "homepi-hifi-serial.service" "/run/homepi/audio/hifi-serial.sock"
fi

if [[ "${AUDIO_CHANGED}" == "1" ]]; then
  deploy_rc=0
  if bash "${SCRIPT_DIR}/deploy-audio-modprobe.sh"; then
    deploy_rc=0
  else
    deploy_rc=$?
  fi

  if [[ "${deploy_rc}" -eq 0 ]]; then
    log "Primary audio modprobe changed — applying stable ALSA name"
  elif [[ "${deploy_rc}" -eq 2 ]]; then
    log "Primary audio modprobe unchanged — applying stable ALSA name if needed"
  elif [[ -f /etc/modprobe.d/homepi-audio-primary.conf ]] \
    && grep -q '^options snd-usb-audio' /etc/modprobe.d/homepi-audio-primary.conf; then
    log "WARN: generated modprobe not ready; using installed /etc/modprobe.d/homepi-audio-primary.conf"
    deploy_rc=2
  else
    log "WARN: primary audio modprobe deploy failed (check Primary Audio Output assignment)"
  fi

  if [[ "${deploy_rc}" -eq 0 || "${deploy_rc}" -eq 2 ]]; then
    if bash "${SCRIPT_DIR}/apply-primary-audio-alsa.sh"; then
      log "HomePiPrimary ALSA name is active"
    else
      schedule_reboot "HomePiPrimary ALSA name requires reboot after primary audio modprobe change" \
        || log "WARN: primary audio apply incomplete; reboot blocked or deferred"
    fi
  fi
fi

if [[ "${AUDIO_CHANGED}" == "1" ]]; then
  restart_audio_dependent_services
  AUDIO_SERVICES_FINALIZED=1
fi

log "Post-assignment hook complete"
