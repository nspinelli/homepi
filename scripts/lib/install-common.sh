#!/usr/bin/env bash
# Shared helpers for HomePi install scripts.
# shellcheck shell=bash

# Resolve repo scripts/lib regardless of caller location.
_INSTALL_COMMON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${REPO_ROOT:-$(cd "${_INSTALL_COMMON_DIR}/../.." && pwd)}"

HOMEPI_BACKUP_ROOT="${HOMEPI_BACKUP_ROOT:-/var/backups/homepi}"
HOMEPI_BACKUP_SESSION="${HOMEPI_BACKUP_SESSION:-}"

log() {
  echo "==> $*"
}

die() {
  echo "ERROR: $*" >&2
  exit 1
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    die "Re-run with sudo."
  fi
}

init_backup_session() {
  if [[ -n "${HOMEPI_BACKUP_SESSION}" ]]; then
    return 0
  fi
  HOMEPI_BACKUP_SESSION="$(date -u +%Y%m%d-%H%M%S)"
  export HOMEPI_BACKUP_SESSION
  mkdir -p "${HOMEPI_BACKUP_ROOT}/install-${HOMEPI_BACKUP_SESSION}"
  log "Backup session: ${HOMEPI_BACKUP_ROOT}/install-${HOMEPI_BACKUP_SESSION}"
}

backup_system_file() {
  local src="$1"
  init_backup_session
  local dest_dir="${HOMEPI_BACKUP_ROOT}/install-${HOMEPI_BACKUP_SESSION}"
  if [[ ! -e "${src}" ]]; then
    return 0
  fi
  local rel="${src#/}"
  local dest="${dest_dir}/${rel}"
  mkdir -p "$(dirname "${dest}")"
  cp -a "${src}" "${dest}"
  log "Backed up ${src}"
}

install_sudoers_dropin() {
  local src="$1"
  local dest="$2"
  local tmp
  tmp="$(mktemp)"
  install -m 0440 "${src}" "${tmp}"
  if ! visudo -cf "${tmp}"; then
    rm -f "${tmp}"
    die "sudoers validation failed for ${src}"
  fi
  install -m 0440 "${tmp}" "${dest}"
  rm -f "${tmp}"
  if ! visudo -c; then
    rm -f "${dest}"
    die "full sudoers validation failed after installing ${dest}"
  fi
  log "Installed sudoers drop-in ${dest}"
}

remove_sudoers_dropin() {
  local dest="$1"
  if [[ -f "${dest}" ]]; then
    rm -f "${dest}"
    log "Removed sudoers drop-in ${dest}"
    visudo -c >/dev/null 2>&1 || die "sudoers broken after removing ${dest}"
  fi
}

ensure_apt_packages() {
  if [[ "${HOMEPI_SKIP_PREREQS:-0}" == "1" ]]; then
    return 0
  fi
  local missing=()
  local pkg
  for pkg in "$@"; do
    if ! dpkg -l "${pkg}" 2>/dev/null | grep -q ^ii; then
      missing+=("${pkg}")
    fi
  done
  if [[ ${#missing[@]} -eq 0 ]]; then
    return 0
  fi
  log "Installing apt packages: ${missing[*]}"
  apt-get update -qq
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${missing[@]}"
}

assert_command() {
  local cmd
  for cmd in "$@"; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
      die "Required command not found: ${cmd}. Run scripts/install-prerequisites.sh first."
    fi
  done
}

assert_node_version() {
  if ! command -v node >/dev/null 2>&1; then
    die "Node.js >= 20 required. Install Node 20+ before running install."
  fi
  local major
  major="$(node -p "process.versions.node.split('.')[0]")"
  if [[ "${major}" -lt 20 ]]; then
    die "Node.js >= 20 required (found $(node -v))."
  fi
}

assert_pnpm_version() {
  if ! command -v pnpm >/dev/null 2>&1; then
    die "pnpm >= 9 required. Install with: corepack enable && corepack prepare pnpm@9.15.0 --activate"
  fi
  local major
  major="$(pnpm -v | cut -d. -f1)"
  if [[ "${major}" -lt 9 ]]; then
    die "pnpm >= 9 required (found $(pnpm -v))."
  fi
}

assert_patch_applies() {
  local patch_file="$1"
  local src_dir="$2"
  if [[ ! -f "${patch_file}" ]]; then
    die "Patch file not found: ${patch_file}"
  fi
  if [[ ! -d "${src_dir}" ]]; then
    die "Source directory not found: ${src_dir}"
  fi
  if ! patch --dry-run -d "${src_dir}" -p1 < "${patch_file}" >/dev/null 2>&1; then
    die "Patch does not apply cleanly: ${patch_file} (upstream source may have changed)"
  fi
}

apply_patch() {
  local patch_file="$1"
  local src_dir="$2"
  assert_patch_applies "${patch_file}" "${src_dir}"
  patch -d "${src_dir}" -p1 < "${patch_file}"
  log "Applied patch ${patch_file}"
}

read_json_field() {
  local file="$1"
  local field="$2"
  python3 - "${file}" "${field}" <<'PY'
import json
import sys

path, field = sys.argv[1], sys.argv[2]
with open(path, encoding="utf-8") as fh:
    data = json.load(fh)

parts = field.split(".")
value = data
for part in parts:
    if not isinstance(value, dict) or part not in value:
        sys.exit(1)
    value = value[part]

if isinstance(value, (dict, list)):
    print(json.dumps(value))
else:
    print(value)
PY
}

ensure_build_deps_skip_if_prereqs() {
  if [[ "${HOMEPI_SKIP_PREREQS:-0}" == "1" ]]; then
    return 0
  fi
  ensure_apt_packages "$@"
}

chown_homepi_runtime() {
  if id homepi >/dev/null 2>&1; then
    chown -R homepi:homepi /opt/homepi/runtime 2>/dev/null || true
    local dir
    for dir in /opt/homepi/services/*/env /opt/homepi/services/*/storage; do
      [[ -d "${dir}" ]] && chown -R homepi:homepi "${dir}"
    done
  fi
}

install_root_owned_scripts() {
  local dest_dir="$1"
  shift
  install -d -m 0755 "${dest_dir}"
  local src
  for src in "$@"; do
    install -m 0755 -o root -g root "${src}" "${dest_dir}/$(basename "${src}")"
  done
}
