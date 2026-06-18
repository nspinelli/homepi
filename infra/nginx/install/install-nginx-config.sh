#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
HOMEPI_ROOT="${HOMEPI_ROOT:-${REPO_ROOT}}"
HOMEPI_WEB_ROOT="${HOMEPI_WEB_ROOT:-${HOMEPI_ROOT}/apps/frontend/dist}"
HOMEPI_BACKEND_UPSTREAM="${HOMEPI_BACKEND_UPSTREAM:-127.0.0.1:3000}"
HOMEPI_SERVER_NAME="${HOMEPI_SERVER_NAME:-homepi.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE="${SCRIPT_DIR}/../templates/homepi.nginx.conf.template"
OUTPUT="/etc/nginx/sites-available/homepi.local"

if [[ ! -f "${TEMPLATE}" ]]; then
  echo "Template not found: ${TEMPLATE}" >&2
  exit 1
fi

if [[ ! -d "${HOMEPI_WEB_ROOT}" ]]; then
  echo "Frontend dist not found. Run: pnpm --filter @homepi/app-frontend build" >&2
  exit 1
fi

if ! command -v nginx >/dev/null 2>&1; then
  echo "Installing nginx..."
  sudo apt-get update -qq
  sudo apt-get install -y nginx
fi

sed \
  -e "s|\${HOMEPI_WEB_ROOT}|${HOMEPI_WEB_ROOT}|g" \
  -e "s|\${HOMEPI_BACKEND_UPSTREAM}|${HOMEPI_BACKEND_UPSTREAM}|g" \
  -e "s|\${HOMEPI_SERVER_NAME}|${HOMEPI_SERVER_NAME}|g" \
  "${TEMPLATE}" | sudo tee "${OUTPUT}" > /dev/null

# Allow nginx (www-data) to traverse home directory and read static assets
chmod o+x /home/homepi 2>/dev/null || sudo chmod o+x /home/homepi
if ! chmod -R o+rX "${HOMEPI_WEB_ROOT}" 2>/dev/null; then
  sudo chmod -R o+rX "${HOMEPI_WEB_ROOT}"
fi

sudo rm -f /etc/nginx/sites-enabled/default 2>/dev/null || true
sudo ln -sf "${OUTPUT}" /etc/nginx/sites-enabled/homepi.local
sudo nginx -t
sudo systemctl enable nginx
sudo systemctl restart nginx

echo "Installed NGINX config for ${HOMEPI_SERVER_NAME} -> ${OUTPUT}"
echo "  web root: ${HOMEPI_WEB_ROOT}"
echo "  backend:  ${HOMEPI_BACKEND_UPSTREAM}"
