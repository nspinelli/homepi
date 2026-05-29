#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="homepi-pcm-router"
INSTALL_ROOT="/opt/homepi/services/pcm-router"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo" >&2
    exit 1
  fi
}

main() {
  require_root
  systemctl stop "${SERVICE_NAME}.service" 2>/dev/null || true
  systemctl disable "${SERVICE_NAME}.service" 2>/dev/null || true
  rm -f "${UNIT_DEST}"
  systemctl daemon-reload
  rm -rf "${INSTALL_ROOT}"
  rm -f /run/homepi/pcm-router.sock
  echo "Uninstalled ${SERVICE_NAME}"
}

main "$@"
