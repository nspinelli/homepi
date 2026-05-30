#!/usr/bin/env bash
# Per-zone metadata pipe handler: route active_start/end to MQTT, drain idle pipes.
set -euo pipefail

ZONE="${1:-}"
if [[ -z "${ZONE}" ]]; then
  echo "Usage: run-metadata-reader.sh <zone-id>" >&2
  exit 1
fi

INSTALL_ROOT="/opt/homepi/services/metadata"
HANDLER="${INSTALL_ROOT}/bin/metadata-zone-handler.py"

if [[ ! -x "${HANDLER}" ]]; then
  echo "Metadata handler missing: ${HANDLER}" >&2
  exit 1
fi

export HOMEPI_METADATA_PIPE="${HOMEPI_METADATA_PIPE:-/tmp/homepi-metadata-zone-${ZONE}}"
export MQTT_TOPIC="${MQTT_TOPIC:-shairport/zone/${ZONE}}"
export MQTT_HOST="${MQTT_HOST:-127.0.0.1}"
export HOMEPI_EVENT_SOCKET="${HOMEPI_EVENT_SOCKET:-/run/homepi/pcm-router.sock}"
export HOMEPI_METADATA_PIPE_WAIT_SECS="${HOMEPI_METADATA_PIPE_WAIT_SECS:-120}"

exec python3 "${HANDLER}" "${ZONE}"
