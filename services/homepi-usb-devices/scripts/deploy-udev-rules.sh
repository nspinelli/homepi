#!/usr/bin/env bash
# Installs generated udev rules and creates /dev/vHifi.
set -euo pipefail

RULES_SRC="/opt/homepi/runtime/generated/udev/99-homepi-usb-devices.rules"
RULES_DEST="/etc/udev/rules.d/99-homepi-usb-devices.rules"

if [[ "${EUID}" -ne 0 ]]; then
  echo "Re-run with sudo: sudo bash $0" >&2
  exit 1
fi

if [[ ! -f "${RULES_SRC}" ]]; then
  echo "No generated rules at ${RULES_SRC}. Save USB assignments first." >&2
  exit 1
fi

install -m 0644 "${RULES_SRC}" "${RULES_DEST}"
udevadm control --reload-rules
udevadm trigger --subsystem-match=tty --action=add

sleep 1
if [[ -e /dev/vHifi ]]; then
  echo "OK  /dev/vHifi -> $(readlink -f /dev/vHifi)"
else
  echo "WARN /dev/vHifi not present yet. Check serial assignment and FTDI connection." >&2
  exit 1
fi
