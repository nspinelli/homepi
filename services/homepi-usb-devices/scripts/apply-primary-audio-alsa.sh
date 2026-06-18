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

primary_card_index() {
  awk -v id="${PRIMARY_CARD_ID}" '
    $0 ~ "\\[" id "[[:space:]]*\\]" { print $1; exit }
  ' /proc/asound/cards 2>/dev/null
}

primary_card_usb_ids() {
  local card_idx="$1"
  local card_sysfs="/sys/class/sound/card${card_idx}"
  local usb_dev="${card_sysfs}/device"
  local depth=0
  while [[ -L "${usb_dev}" || -d "${usb_dev}" ]] && (( depth < 8 )); do
    if [[ -f "${usb_dev}/idVendor" && -f "${usb_dev}/idProduct" ]]; then
      echo "$(tr '[:upper:]' '[:lower:]' < "${usb_dev}/idVendor") $(tr '[:upper:]' '[:lower:]' < "${usb_dev}/idProduct")"
      return 0
    fi
    usb_dev="${usb_dev}/.."
    depth=$((depth + 1))
  done
  return 1
}

primary_card_matches_modprobe() {
  local card_idx card_vid card_pid
  card_idx=$(primary_card_index)
  if [[ -z "${card_idx}" ]]; then
    return 1
  fi
  if ! read -r card_vid card_pid < <(primary_card_usb_ids "${card_idx}"); then
    return 1
  fi
  [[ "${card_vid}" == "${VID}" && "${card_pid}" == "${PID}" ]]
}

usb_port_from_sound_card() {
  local card_idx="$1"
  local node="/sys/class/sound/card${card_idx}/device"
  local depth=0
  while [[ -L "${node}" || -d "${node}" ]] && (( depth < 10 )); do
    local name
    name=$(basename "${node}")
    if [[ "${name}" =~ ^[0-9]+-[0-9]+$ ]]; then
      echo "${name}"
      return 0
    fi
    node="${node}/.."
    depth=$((depth + 1))
  done
  return 1
}

usb_port_has_audio_interface() {
  local port="$1"
  local iface class
  for iface in /sys/bus/usb/devices/"${port}"/"${port}":*; do
    [[ -f "${iface}/bInterfaceClass" ]] || continue
    class=$(cat "${iface}/bInterfaceClass")
    if [[ "${class}" == "01" ]]; then
      return 0
    fi
  done
  return 1
}

list_usb_audio_ports() {
  local dev port
  for dev in /sys/bus/usb/devices/*; do
    [[ "${dev}" =~ /[0-9]+-[0-9]+$ ]] || continue
    [[ -f "${dev}/idVendor" && -f "${dev}/idProduct" ]] || continue
    port=$(basename "${dev}")
    if usb_port_has_audio_interface "${port}"; then
      echo "${port}"
    fi
  done
}

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

stop_audio_consumers() {
  local unit
  for unit in homepi-pcm-router.service homepi-shairport-supervisor.service; do
    if systemctl is-active "${unit}" >/dev/null 2>&1; then
      log "Stopping ${unit} during ALSA reload"
      systemctl stop "${unit}"
    fi
  done
}

reload_usb_audio_for_primary() {
  local primary_port="$1"
  local port
  local -a audio_ports=()

  while IFS= read -r port; do
    [[ -n "${port}" ]] && audio_ports+=("${port}")
  done < <(list_usb_audio_ports)

  log "Reloading USB audio so ${PRIMARY_CARD_ID} can bind to ${VID}:${PID}"

  stop_audio_consumers

  modprobe -r snd-usb-audio 2>/dev/null || true
  for port in "${audio_ports[@]}"; do
    echo "${port}" > /sys/bus/usb/drivers/usb/unbind 2>/dev/null || true
  done
  sleep 2

  echo "${primary_port}" > /sys/bus/usb/drivers/usb/bind
  sleep 3

  for port in "${audio_ports[@]}"; do
    [[ -n "${port}" && "${port}" != "${primary_port}" ]] || continue
    echo "${port}" > /sys/bus/usb/drivers/usb/bind 2>/dev/null || true
    sleep 1
  done

  modprobe snd-usb-audio 2>/dev/null || true
  rebind_stranded_usb_ports
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
    if has_primary_card && primary_card_matches_modprobe; then
      return 0
    fi
    sleep 1
    elapsed=$((elapsed + 1))
  done
  return 1
}

main() {
  require_root

  parse_modprobe_ids || exit 1

  if has_primary_card && primary_card_matches_modprobe; then
    log "${PRIMARY_CARD_ID} already present for ${VID}:${PID}"
    exit 0
  fi

  local usb_name
  if ! usb_name=$(find_usb_sysfs_name); then
    log "USB device ${VID}:${PID} not found in sysfs"
    exit 1
  fi

  if has_primary_card && ! primary_card_matches_modprobe; then
    log "${PRIMARY_CARD_ID} is bound to a different USB device — reloading USB audio"
    reload_usb_audio_for_primary "${usb_name}"
  elif ! has_primary_card; then
    log "Stable ALSA name ${PRIMARY_CARD_ID} missing — reloading USB audio"
    reload_usb_audio_for_primary "${usb_name}"
  else
    rebind_usb_device "${usb_name}"
  fi

  if wait_for_primary_card; then
    log "${PRIMARY_CARD_ID} detected after USB apply"
    aplay -l | grep -i "${PRIMARY_CARD_ID}" || true
    exit 0
  fi

  if has_primary_card; then
    log "${PRIMARY_CARD_ID} not detected after ${WAIT_SEC}s"
    exit 1
  fi

  log "Rebind did not apply ${PRIMARY_CARD_ID} — retrying with full USB audio reload"
  reload_usb_audio_for_primary "${usb_name}"

  if wait_for_primary_card; then
    log "${PRIMARY_CARD_ID} detected after USB audio reload"
    aplay -l | grep -i "${PRIMARY_CARD_ID}" || true
    exit 0
  fi

  log "${PRIMARY_CARD_ID} not detected after ${WAIT_SEC}s"
  exit 1
}

main "$@"
