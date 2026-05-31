#!/usr/bin/env bash
# Builds HomePi native daemons that emit event-driven service status.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Services with C/C++ sources updated for system.service events.
NATIVE_SERVICES=(
  "homepi-usb-devices"
  "homepi-hifi-serial"
  "homepi-pcm-router"
)

log() {
  echo "==> $*"
}

build_service() {
  local dir="$1"
  local service_root="${REPO_ROOT}/services/${dir}"
  local build_dir="${service_root}/build"

  if [[ ! -f "${service_root}/CMakeLists.txt" ]]; then
    echo "Missing CMakeLists.txt: ${service_root}" >&2
    exit 1
  fi

  log "Building ${dir}"
  cmake -S "${service_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release
  cmake --build "${build_dir}" --parallel "$(nproc 2>/dev/null || echo 2)"
}

main() {
  log "Building native services from ${REPO_ROOT}/services"
  for dir in "${NATIVE_SERVICES[@]}"; do
    build_service "${dir}"
  done
  log "Native service builds complete"
}

main "$@"
