#!/usr/bin/env bash
# Phase 10 legacy cleanup helper — disables removed services and flat socket symlinks.
set -euo pipefail

echo "==> Disabling legacy homepi-events"
sudo systemctl stop homepi-events.service 2>/dev/null || true
sudo systemctl disable homepi-events.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/homepi-events.service

echo "==> Removing legacy flat socket symlinks under /run/homepi"
for legacy in events.sock usb-devices.sock hifi-serial.sock pcm-router.sock metadata.sock audio-realtime.sock audio-paging.sock; do
  sudo rm -f "/run/homepi/${legacy}"
done

sudo systemctl daemon-reload
echo "Legacy cleanup complete."
