#!/usr/bin/env bash
# Installs operational HomePi stack: build, backend service, NGINX, mDNS alias.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${REPO_ROOT}"

# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

export HOMEPI_INSTALL_MODE=1
export HOMEPI_ALLOW_REBOOT=0
export HOMEPI_SKIP_PREREQS=1

echo "==> Running install preflight (backups, SSH checks)"
sudo bash "${REPO_ROOT}/scripts/install-preflight.sh"

echo "==> Installing SSH boot hardening"
sudo bash "${REPO_ROOT}/scripts/install-ensure-ssh.sh"

echo "==> Installing prerequisites"
sudo bash "${REPO_ROOT}/scripts/install-prerequisites.sh"

echo "==> Installing Node dependencies and building HomePi monorepo"
assert_node_version
assert_pnpm_version
if command -v pnpm >/dev/null 2>&1; then
  pnpm install
  pnpm run build
else
  npx pnpm install
  npx pnpm run build
fi

echo "==> Installing core/events broker"
sudo bash "${REPO_ROOT}/core/events/scripts/install.sh"

echo "==> Installing HomePi native services"
sudo env HOMEPI_INSTALL_MODE=1 HOMEPI_ALLOW_REBOOT=0 HOMEPI_SKIP_PREREQS=1 \
  bash "${REPO_ROOT}/scripts/install-services.sh"

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
bash "${REPO_ROOT}/scripts/verify-operational.sh"

echo ""
echo "Operational HomePi is ready at http://homepi.local (mDNS on LAN)"
echo "  Pi IP: $(hostname -I | awk '{print $1}')"
echo "  Other devices on the network can open http://homepi.local"
echo "  Backups: ${HOMEPI_BACKUP_ROOT:-/var/backups/homepi}/ (latest preflight session)"
