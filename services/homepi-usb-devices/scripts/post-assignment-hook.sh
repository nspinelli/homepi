#!/usr/bin/env bash
# Deploys udev rules and restarts homepi-hifi-serial after USB assignment save.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

bash "${SCRIPT_DIR}/deploy-udev-rules.sh"

if systemctl is-enabled homepi-hifi-serial.service >/dev/null 2>&1; then
  systemctl restart homepi-hifi-serial.service
fi
