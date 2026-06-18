#!/usr/bin/env bash
# Installs all HomePi build and runtime apt dependencies in one transaction.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

ENSURE_DIALOUT=0

APT_PACKAGES=(
  nodejs
  git autoconf automake libtool pkg-config build-essential cmake g++ xxd
  libudev-dev libsqlite3-dev libasound2-dev alsa-utils libmosquitto-dev
  libpopt-dev libconfig-dev libavahi-client-dev libssl-dev libsoxr-dev
  libplist-dev libsodium-dev libavutil-dev libavcodec-dev libavformat-dev
  uuid-dev libgcrypt20-dev sqlite3 netcat-openbsd
  mosquitto mosquitto-clients nginx avahi-daemon avahi-utils libnss-mdns curl
)

usage() {
  cat <<'EOF'
Usage: sudo bash scripts/install-prerequisites.sh [--ensure-dialout]

Installs all apt packages required by HomePi native services and runtime.
Verifies Node >= 20 and pnpm >= 9 (not installed via apt).
EOF
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --ensure-dialout) ENSURE_DIALOUT=1; shift ;;
      -h|--help) usage; exit 0 ;;
      *) die "Unknown option: $1" ;;
    esac
  done
}

ensure_dialout_group() {
  if [[ "${ENSURE_DIALOUT}" -ne 1 ]]; then
    if id homepi >/dev/null 2>&1 && ! groups homepi | grep -q '\bdialout\b'; then
      echo "WARN: user homepi is not in group dialout (required for hifi-serial)." >&2
      echo "      Re-run with --ensure-dialout or: sudo usermod -aG dialout homepi" >&2
    fi
    return 0
  fi
  if id homepi >/dev/null 2>&1; then
    if ! groups homepi | grep -q '\bdialout\b'; then
      usermod -aG dialout homepi
      log "Added homepi to dialout group (log out/in or reboot for effect)"
    fi
  fi
}

enable_mosquitto() {
  systemctl enable mosquitto
  systemctl start mosquitto
  log "mosquitto enabled and started"
}

setup_node_tooling() {
  if ! command -v node >/dev/null 2>&1; then
    die "nodejs package installed but node binary not found"
  fi
  assert_node_version
  if ! command -v pnpm >/dev/null 2>&1; then
    log "Enabling corepack and activating pnpm@9.15.0"
    corepack enable
    corepack prepare pnpm@9.15.0 --activate
  fi
  assert_pnpm_version
}

main() {
  parse_args "$@"
  require_root
  log "Installing HomePi prerequisites"
  ensure_apt_packages "${APT_PACKAGES[@]}"
  setup_node_tooling
  enable_mosquitto
  ensure_dialout_group

  echo ""
  echo "Prerequisites installed. HOMEPI_SKIP_PREREQS=1 for subsequent service installs."
}

main "$@"
