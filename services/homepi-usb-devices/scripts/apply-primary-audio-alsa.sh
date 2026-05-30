#!/usr/bin/env bash
# Applies HomePiPrimary by rebinding the assigned USB audio device.
# Exit 0 when /proc/asound/cards lists HomePiPrimary.
# Exit 1 when the stable ALSA name is still missing (caller should reboot).
set -euo pipefail

MODPROBE_DEST="/etc/modprobe.d/homepi-audio-primary.conf"
PRIMARY_CARD_ID="${HOMEPI_PRIMARY_ALSA_CARD:-HomePiPrimary}"
WAIT_SEC="${HOMEPI_ALSA_APPLY_WAIT_SEC:-20}"

log() { echo "==> $*"; }

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash $0" >&2
    exit 1
  fi
}

parse_modprobe_ids() {
  if [[ ! -f "${MODPROBE_DEST}" ]]; then
    log "Missing ${MODPROBE_DEST}"
    return 1
  fi
  local line
  line=$(grep -E '^options snd-usb-audio' "${MODPROBE_DEST}" | head -1 || true)
  if [[ -z "${line}" ]]; then
    log "No snd-usb-audio options in ${MODPROBE_DEST}"
    return 1
  fi
  MODPROBE_LINE="${line}"
  VID=$(echo "${line}" | sed -n 's/.* vid=0x\([0-9a-fA-F]\+\).*/\1/p' | tr '[:upper:]' '[:lower:]')
  PID=$(echo "${line}" | sed -n 's/.* pid=0x\([0-9a-fA-F]\+\).*/\1/p' | tr '[:upper:]' '[:lower:]')
  if [[ -z "${VID}" || -z "${PID}" ]]; then
    log "Could not parse vid/pid from modprobe options"
    return 1
  fi
}

has_primary_card() {
  grep -q "\[${PRIMARY_CARD_ID}[[:space:]]*\]" /proc/asound/cards 2>/dev/null
}

find_usb_sysfs_name() {
  local dev vendor product
  for dev in /sys/bus/usb/devices/*; do
    [[ -f "${dev}/idVendor" && -f "${dev}/idProduct" ]] || continue
    vendor=$(tr '[:upper:]' '[:lower:]' < "${dev}/idVendor")
    product=$(tr '[:upper:]' '[:lower:]' < "${dev}/idProduct")
    if [[ "${vendor}" == "${VID}" && "${product}" == "${PID}" ]]; then
      basename "${dev}"
      return 0
    fi
  done
  return 1
}

rebind_usb_device() {
  local usb_name="$1"
  local unbind="/sys/bus/usb/drivers/usb/unbind"
  local bind="/sys/bus/usb/drivers/usb/bind"
  if [[ ! -w "${unbind}" || ! -w "${bind}" ]]; then
    log "USB sysfs bind nodes not writable"
    return 1
  fi
  log "Rebinding USB device ${usb_name} to apply ${PRIMARY_CARD_ID}"
  echo "${usb_name}" > "${unbind}" 2>/dev/null || true
  sleep 2
  echo "${usb_name}" > "${bind}" 2>/dev/null || true
}

wait_for_primary_card() {
  local elapsed=0
  while (( elapsed < WAIT_SEC )); do
    if has_primary_card; then
      return 0
    fi
    sleep 1
    elapsed=$((elapsed + 1))
  done
  return 1
}

main() {
  require_root

  if has_primary_card; then
    log "${PRIMARY_CARD_ID} already present"
    exit 0
  fi

  parse_modprobe_ids || exit 1

  local usb_name
  if ! usb_name=$(find_usb_sysfs_name); then
    log "USB device ${VID}:${PID} not found in sysfs"
    exit 1
  fi

  rebind_usb_device "${usb_name}"
  if wait_for_primary_card; then
    log "${PRIMARY_CARD_ID} detected after USB rebind"
    aplay -l | grep -i "${PRIMARY_CARD_ID}" || true
    exit 0
  fi

  log "${PRIMARY_CARD_ID} not detected after ${WAIT_SEC}s"
  exit 1
}

main "$@"
