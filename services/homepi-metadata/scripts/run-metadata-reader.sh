#!/usr/bin/env bash
# Reads Shairport Sync metadata from a named pipe into journald (stdout).
set -euo pipefail

INSTALL_ROOT="/opt/homepi/services/metadata"
READER="${INSTALL_ROOT}/bin/shairport-sync-metadata-reader"
PIPE="${HOMEPPI_METADATA_PIPE:-/tmp/shairport-sync-metadata}"
RAW="${HOMEPPI_METADATA_RAW:-0}"
WAIT_SECS="${HOMEPPI_METADATA_PIPE_WAIT_SECS:-120}"

if [[ ! -x "${READER}" ]]; then
  echo "Metadata reader binary missing: ${READER}" >&2
  exit 1
fi

wait_for_pipe() {
  local elapsed=0
  while [[ ! -e "${PIPE}" ]] && [[ "${elapsed}" -lt "${WAIT_SECS}" ]]; do
    sleep 1
    elapsed=$((elapsed + 1))
  done
}

ensure_pipe() {
  wait_for_pipe
  if [[ ! -e "${PIPE}" ]]; then
    local dir
    dir="$(dirname "${PIPE}")"
    mkdir -p "${dir}"
    mkfifo "${PIPE}" 2>/dev/null || true
  fi
  if [[ ! -e "${PIPE}" ]]; then
    echo "Metadata pipe unavailable: ${PIPE}" >&2
    exit 1
  fi
}

ensure_pipe

args=()
if [[ "${RAW}" == "1" ]] || [[ "${RAW}" == "true" ]]; then
  args+=(--raw)
fi

exec "${READER}" "${args[@]}" < "${PIPE}"
