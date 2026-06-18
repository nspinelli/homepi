#!/usr/bin/env bash
# Installs generated modprobe options for stable primary USB audio card ID.
# Exit 0: installed and content changed (ALSA reload/reboot may be required).
# Exit 2: already up to date (no ALSA reload required).
# Exit 1: error or no primary assignment.
set -euo pipefail

MODPROBE_SRC="/opt/homepi/runtime/generated/modprobe/homepi-audio-primary.conf"
MODPROBE_DEST="/etc/modprobe.d/homepi-audio-primary.conf"

if [[ "${HOMEPI_INSTALL_MODE:-0}" == "1" ]]; then
  echo "Skipping primary audio modprobe deploy during install (HOMEPI_INSTALL_MODE=1)" >&2
  exit 2
fi

if [[ "${EUID}" -ne 0 ]]; then
  echo "Re-run with sudo: sudo bash $0" >&2
  exit 1
fi

if [[ ! -f "${MODPROBE_SRC}" ]]; then
  echo "No generated modprobe config at ${MODPROBE_SRC}. Save USB assignments first." >&2
  exit 1
fi

if grep -q '^# No primary audio assignment' "${MODPROBE_SRC}"; then
  echo "Removing stale primary audio modprobe (no assignment)"
  if [[ -f "${MODPROBE_DEST}" ]]; then
    rm -f "${MODPROBE_DEST}"
    echo "Removed ${MODPROBE_DEST}"
    exit 0
  fi
  exit 2
fi

if grep -q '^#' "${MODPROBE_SRC}" && ! grep -q '^options snd-usb-audio' "${MODPROBE_SRC}"; then
  echo "Primary audio modprobe not ready: ${MODPROBE_SRC}" >&2
  exit 1
fi

if [[ -f "${MODPROBE_DEST}" ]] && cmp -s "${MODPROBE_SRC}" "${MODPROBE_DEST}"; then
  echo "OK  ${MODPROBE_DEST} unchanged"
  exit 2
fi

install -m 0644 "${MODPROBE_SRC}" "${MODPROBE_DEST}"
echo "OK  ${MODPROBE_DEST} installed (content changed)"
