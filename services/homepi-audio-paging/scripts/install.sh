#!/usr/bin/env bash
set -euo pipefail

SERVICE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "${SERVICE_ROOT}/../.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

SERVICE_NAME="homepi-audio-paging"
INSTALL_ROOT="/opt/homepi/services/audio-paging"
RUNTIME_ROOT="/opt/homepi/runtime"
PAGING_ROOT="/var/lib/homepi/paging"
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
  ensure_build_deps_skip_if_prereqs cmake g++ pkg-config libsqlite3-dev
}

build_binary() {
  log "Building ${SERVICE_NAME}"
  mkdir -p "${BUILD_DIR}"
  cmake -S "${SERVICE_ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
  cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 2)"
}

install_files() {
  log "Installing files to ${INSTALL_ROOT}"
  install -d -m 0755 "${INSTALL_ROOT}/bin"
  install -d -m 0755 "${INSTALL_ROOT}/config"
  install -d -m 0755 "${INSTALL_ROOT}/storage/migrations"
  install -d -m 0755 "${INSTALL_ROOT}/assets/chimes"
  install -d -m 0755 "${INSTALL_ROOT}/assets/voices"
  install -d -m 0755 "${INSTALL_ROOT}/scripts"
  install -d -m 0755 "${INSTALL_ROOT}/env"
  install -d -m 0755 "${RUNTIME_ROOT}/state"
  install -d -m 0755 /run/homepi
  install -d -m 0755 "${PAGING_ROOT}/voices"
  install -d -m 0755 "${PAGING_ROOT}/chimes"

  install -m 0755 "${BUILD_DIR}/homepi-audio-paging" "${INSTALL_ROOT}/bin/homepi-audio-paging"
  install -m 0644 "${SERVICE_ROOT}/config/service-config.json" "${INSTALL_ROOT}/config/service-config.json"
  install -m 0644 "${SERVICE_ROOT}/storage/migrations/001-audio-paging.sql" \
    "${INSTALL_ROOT}/storage/migrations/001-audio-paging.sql"
  install -m 0644 "${SERVICE_ROOT}/assets/voices/catalog.json" "${INSTALL_ROOT}/assets/voices/catalog.json"
  if [[ -f "${SERVICE_ROOT}/assets/chimes/default.wav" ]]; then
    install -m 0644 "${SERVICE_ROOT}/assets/chimes/default.wav" "${INSTALL_ROOT}/assets/chimes/default.wav"
  fi
}

ensure_default_chime() {
  local target="${PAGING_ROOT}/chimes/default.wav"
  if [[ -f "${target}" ]]; then
    return
  fi
  log "Generating placeholder default chime at ${target}"
  python3 - <<'PY'
import math
import struct
import wave

sample_rate = 22050
duration_sec = 0.2
frequency = 880.0
frames = int(sample_rate * duration_sec)
path = "/var/lib/homepi/paging/chimes/default.wav"
with wave.open(path, "wb") as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(sample_rate)
    for i in range(frames):
        amp = int(12000 * math.sin(2 * math.pi * frequency * (i / sample_rate)))
        wav.writeframesraw(struct.pack("<h", amp))
PY
}

install_piper_runtime() {
  local runtime_dir="${INSTALL_ROOT}/bin/piper-runtime"
  local archive="/tmp/piper_linux_aarch64.tar.gz"
  if [[ -x "${runtime_dir}/piper" ]]; then
    log "Piper runtime already installed"
    return
  fi
  log "Installing Piper ARM64 runtime"
  curl -fsSL -o "${archive}" \
    "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_linux_aarch64.tar.gz"
  rm -rf "${runtime_dir}"
  mkdir -p "${runtime_dir}"
  tar -xzf "${archive}" -C "${runtime_dir}" --strip-components=1
  chmod 0755 "${runtime_dir}/piper"
}

ensure_default_voice() {
  local voice_dir="${PAGING_ROOT}/voices"
  local model="${voice_dir}/en_US-lessac-medium.onnx"
  local config="${voice_dir}/en_US-lessac-medium.onnx.json"
  if [[ -f "${model}" && -f "${config}" ]]; then
    log "Default voice already present"
    return
  fi
  log "Downloading bundled default voice en_US-lessac-medium"
  local base="https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium"
  curl -fsSL -o "${model}" "${base}/en_US-lessac-medium.onnx"
  curl -fsSL -o "${config}" "${base}/en_US-lessac-medium.onnx.json"
}

sync_alsa_user_config() {
  log "Syncing ALSA user config for plug:AudioPaging"
  bash "${SERVICE_ROOT}/scripts/sync-alsa-user-config.sh"
}

install_systemd() {
  log "Installing systemd unit"
  install -m 0644 "${UNIT_SRC}" "${UNIT_DEST}"
  systemctl daemon-reload
  systemctl enable "${SERVICE_NAME}.service"
  systemctl restart "${SERVICE_NAME}.service"
}

verify_install() {
  log "Verifying installation"
  sleep 2
  systemctl is-active "${SERVICE_NAME}.service" >/dev/null
  if [[ ! -S /run/homepi/audio/paging.sock ]]; then
    echo "Socket missing: /run/homepi/audio/paging.sock" >&2
    journalctl -u "${SERVICE_NAME}.service" -n 30 --no-pager >&2 || true
    exit 1
  fi
  echo "  OK  ${SERVICE_NAME} active, socket healthy"
}

main() {
  require_root
  ensure_build_deps
  build_binary
  install_files
  install_piper_runtime
  ensure_default_voice
  ensure_default_chime
  sync_alsa_user_config
  chown -R homepi:homepi "${INSTALL_ROOT}" "${PAGING_ROOT}" /run/homepi "${RUNTIME_ROOT}/state" || true
  install_systemd
  verify_install
  echo "${SERVICE_NAME} installed and running."
}

main "$@"
