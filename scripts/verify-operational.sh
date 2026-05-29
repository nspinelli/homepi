#!/usr/bin/env bash
set -euo pipefail

echo "=== HomePi operational status ==="
for svc in nginx homepi-backend avahi-daemon avahi-homepi-alias \
  homepi-usb-devices homepi-nqptp homepi-metadata homepi-hifi-serial; do
  printf "  %-24s %s\n" "${svc}" "$(systemctl is-active "${svc}" 2>/dev/null || echo unknown)"
done

echo ""
echo "=== mDNS ==="
getent hosts homepi.local || true

echo ""
echo "=== HTTP checks ==="
curl -sf -o /dev/null -w "  frontend homepi.local  %{http_code}\n" http://homepi.local/
curl -sf -o /dev/null -w "  api health           %{http_code}\n" http://homepi.local/api/health
curl -sf http://homepi.local/api/health | head -c 120
echo ""
