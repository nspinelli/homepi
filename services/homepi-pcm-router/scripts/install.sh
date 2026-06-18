#!/usr/bin/env bash
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "${SERVICE_ROOT}/../.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"
SERVICE_NAME="homepi-pcm-router"
INSTALL_ROOT="/opt/homepi/services/pcm-router"
RUNTIME_ROOT="/opt/homepi/runtime"
BUILD_DIR="${SERVICE_ROOT}/build"
UNIT_SRC="${SERVICE_ROOT}/systemd/${SERVICE_NAME}.service"
UNIT_DEST="/etc/systemd/system/${SERVICE_NAME}.service"

log() { echo "==> $*"; }

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash ${SERVICE_ROOT}/scripts/install.sh" >&2
    exit 1
  fi
}

ensure_build_deps() {
  ensure_build_deps_skip_if_prereqs cmake g++ pkg-config libasound2-dev alsa-utils
}

install_alsa_loopback() {
  log "Installing ALSA loopback module configuration"
  backup_system_file /etc/modules-load.d/homepi-aloop.conf
  backup_system_file /etc/modprobe.d/homepi-aloop.conf
  install -m 0644 "${SERVICE_ROOT}/config/homepi-aloop-modules-load.conf" \
    /etc/modules-load.d/homepi-aloop.conf
  install -m 0644 "${SERVICE_ROOT}/config/homepi-aloop-modprobe.conf" \
    /etc/modprobe.d/homepi-aloop.conf

  if lsmod | grep -q snd_aloop; then
    log "snd-aloop already loaded; reboot if cards are missing"
  else
    modprobe snd-aloop || true
  fi

  if [[ ! -r /proc/asound/cards ]]; then
    echo "WARN: /proc/asound/cards not readable after snd-aloop load" >&2
    return 0
  fi
  if grep -qE 'HomePiZonesA|HomePiZonesB|Loopback' /proc/asound/cards 2>/dev/null; then
    log "ALSA loopback cards present"
  else
    echo "WARN: HomePi loopback cards not listed yet; reboot may be required" >&2
  fi
}

build_binary() {
  log "Building ${SERVICE_NAME}"
  mkdir -p "${BUILD_DIR}"
  cmake -S "${SERVICE_ROOT}" -B "${BUILD_DIR}"
  cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 2)"
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -R 'test_audio_profile_repository|test_event_emitter'
}

install_files() {
  log "Installing to ${INSTALL_ROOT}"
  install -d -m 0755 "${INSTALL_ROOT}/bin"
  install -d -m 0755 "${INSTALL_ROOT}/config"
  install -d -m 0755 "${INSTALL_ROOT}/env"
  install -d -m 0755 "${RUNTIME_ROOT}/state"
  install -d -m 0755 "${RUNTIME_ROOT}/generated/audio"

  install -m 0755 "${BUILD_DIR}/homepi-pcm-router" "${INSTALL_ROOT}/bin/homepi-pcm-router"
  install -m 0644 "${SERVICE_ROOT}/config/homepi-pcm-router.env.example" \
    "${INSTALL_ROOT}/env/.env.example"
  if [[ ! -f "${INSTALL_ROOT}/env/.env" ]]; then
    install -m 0644 "${SERVICE_ROOT}/config/homepi-pcm-router.env.example" \
      "${INSTALL_ROOT}/env/.env"
  fi
}

install_systemd() {
  log "Installing systemd unit"
  install -m 0644 "${UNIT_SRC}" "${UNIT_DEST}"
  systemctl daemon-reload
  systemctl enable "${SERVICE_NAME}.service"
}

try_start_service() {
  log "Starting ${SERVICE_NAME} (degraded mode OK when DAC is not ready)"
  systemctl restart "${SERVICE_NAME}.service" || true
}

restart_backend() {
  if systemctl is-enabled homepi-backend >/dev/null 2>&1; then
    log "Restarting homepi-backend"
    systemctl restart homepi-backend
  fi
}

main() {
  require_root
  ensure_build_deps
  install_alsa_loopback
  build_binary
  install_files
  install_systemd
  try_start_service
  restart_backend
  log "Installation complete"
}

main "$@"
