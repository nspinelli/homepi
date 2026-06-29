#!/usr/bin/env bash
# Installs all HomePi native services in dependency-safe order.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

# Explicit install order
SERVICE_DIRS=(
  "homepi-usb-devices"
  "homepi-nqptp"
  "homepi-pcm-router"
  "homepi-metadata"
  "homepi-hifi-serial"
  "homepi-shairport-sync"
  "homepi-audio-orchestrator"
  "homepi-audio-paging"
)

log() {
  echo "==> $*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash ${REPO_ROOT}/scripts/install-services.sh" >&2
    exit 1
  fi
}

main() {
  require_root
  log "Installing HomePi native services from ${REPO_ROOT}/services"

  for dir in "${SERVICE_DIRS[@]}"; do
    local install_script="${REPO_ROOT}/services/${dir}/scripts/install.sh"
    if [[ ! -f "${install_script}" ]]; then
      echo "Missing install script: ${install_script}" >&2
      exit 1
    fi
    log "Installing ${dir}"
    bash "${install_script}" || {
      echo "Failed to install ${dir}" >&2
      exit 1
    }
  done

  log "All native services installed"
}

main "$@"
