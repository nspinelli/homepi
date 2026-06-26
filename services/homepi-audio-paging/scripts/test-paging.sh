#!/usr/bin/env bash
set -euo pipefail

SOCKET="${HOMEPI_AUDIO_PAGING_SOCKET:-/run/homepi/audio-paging.sock}"
BACKEND="${HOMEPI_BACKEND_URL:-http://127.0.0.1:3000}"

request_socket() {
  local payload="$1"
  printf '%s\n' "${payload}" | nc -U -w 2 "${SOCKET}"
}

echo "==> Paging socket health"
request_socket '{"method":"getHealth","correlationId":"test-health"}'

echo
echo "==> Paging config"
request_socket '{"method":"getConfig","correlationId":"test-config"}'

echo
echo "==> Backend paging status"
curl -sf "${BACKEND}/api/audio/paging/status" | head -c 400
echo

echo
echo "Paging smoke checks completed."
