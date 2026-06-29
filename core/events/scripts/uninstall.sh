#!/usr/bin/env bash
# Removes orphaned v1 homepi-events install artifacts (runtime decommissioned in v2).
set -euo pipefail

INSTALL_ROOT="/opt/homepi/services/events"

echo "==> Removing orphaned homepi-events install at ${INSTALL_ROOT}"
sudo systemctl stop homepi-events.service 2>/dev/null || true
sudo systemctl disable homepi-events.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/homepi-events.service
sudo rm -rf "${INSTALL_ROOT}"
sudo rm -f /run/homepi/events.sock
sudo systemctl daemon-reload
echo "Orphaned homepi-events artifacts removed."
