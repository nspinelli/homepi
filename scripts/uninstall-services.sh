#!/usr/bin/env bash
# Uninstalls all HomePi native services in reverse dependency order.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Reverse of install-services.sh order.
SERVICE_DIRS=(
  "homepi-audio-paging"
  "homepi-audio-orchestrator"
  "homepi-shairport-sync"
  "homepi-hifi-serial"
  "homepi-metadata"
  "homepi-pcm-router"
  "homepi-nqptp"
  "homepi-usb-devices"
)

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash ${REPO_ROOT}/scripts/uninstall-services.sh" >&2
    exit 1
  fi
}

main() {
  require_root
  log "Uninstalling HomePi native services from ${REPO_ROOT}/services"

  for dir in "${SERVICE_DIRS[@]}"; do
    local uninstall_script="${REPO_ROOT}/services/${dir}/scripts/uninstall.sh"
    if [[ ! -f "${uninstall_script}" ]]; then
      log "Skipping ${dir} (no uninstall.sh)"
      continue
    fi
    log "Uninstalling ${dir}"
    bash "${uninstall_script}" || {
      echo "Failed to uninstall ${dir}" >&2
      exit 1
    }
  done

  log "All native services uninstalled"
}

main "$@"
