#!/usr/bin/env bash
# Phase D/E legacy artifact cleanup — v1 homepi-events and flat socket symlinks.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "==> Disabling legacy homepi-events"
sudo systemctl stop homepi-events.service 2>/dev/null || true
sudo systemctl disable homepi-events.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/homepi-events.service

echo "==> Removing orphaned homepi-events install directory"
sudo rm -rf /opt/homepi/services/events

echo "==> Removing legacy flat socket symlinks under /run/homepi"
for legacy in events.sock usb-devices.sock hifi-serial.sock pcm-router.sock metadata.sock audio-realtime.sock audio-paging.sock; do
  sudo rm -f "/run/homepi/${legacy}"
done

sudo systemctl daemon-reload
echo "Legacy artifact cleanup complete."
