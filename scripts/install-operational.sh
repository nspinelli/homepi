#!/usr/bin/env bash
# Installs operational HomePi stack: build, backend service, NGINX, mDNS alias.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${REPO_ROOT}"

echo "==> Building HomePi monorepo"
if command -v pnpm >/dev/null 2>&1; then
  pnpm run build
else
  npx pnpm run build
fi

echo "==> Installing HomePi native services"
sudo bash "${REPO_ROOT}/scripts/install-services.sh"

echo "==> Installing NGINX site"
HOMEPI_ROOT="${REPO_ROOT}" bash "${REPO_ROOT}/infra/nginx/install/install-nginx-config.sh"

echo "==> Installing backend systemd unit"
sudo cp "${REPO_ROOT}/infra/nginx/install/homepi-backend.service" /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable homepi-backend
sudo systemctl restart homepi-backend

echo "==> Ensuring Avahi mDNS alias for homepi.local"
if ! dpkg -l avahi-daemon 2>/dev/null | grep -q ^ii; then
  sudo apt-get update -qq
  sudo apt-get install -y avahi-daemon avahi-utils libnss-mdns
elif ! command -v avahi-publish-address >/dev/null 2>&1; then
  sudo apt-get update -qq
  sudo apt-get install -y avahi-utils
fi
sudo systemctl enable avahi-daemon
sudo systemctl start avahi-daemon

sudo cp "${REPO_ROOT}/infra/nginx/install/avahi-homepi-alias.service" /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable avahi-homepi-alias
sudo systemctl restart avahi-homepi-alias

echo "==> Verifying services"
sleep 2
systemctl is-active nginx homepi-backend avahi-daemon avahi-homepi-alias

echo "==> Smoke tests"
FAIL=0
check() {
  local label="$1"
  local url="$2"
  local code
  code=$(curl -sf -o /dev/null -w "%{http_code}" "${url}") || code="000"
  if [[ "${code}" == "200" ]]; then
    echo "  OK  ${label} (${code})"
  else
    echo "  FAIL ${label} (${code}) ${url}"
    FAIL=1
  fi
}

check "API health (localhost)" "http://127.0.0.1/api/health"
check "Frontend (localhost)" "http://127.0.0.1/"
check "Frontend (homepi.local)" "http://homepi.local/"
check "API health (homepi.local)" "http://homepi.local/api/health"

if [[ "${FAIL}" -ne 0 ]]; then
  echo "Smoke tests failed. Check: journalctl -u homepi-backend -u nginx" >&2
  exit 1
fi

echo ""
echo "Operational HomePi is ready at http://homepi.local (mDNS on LAN)"
echo "  Pi IP: $(hostname -I | awk '{print $1}')"
echo "  Other devices on the network can open http://homepi.local"
