#!/usr/bin/env bash
# Verifies contact sensors module operational readiness on a HomePi host.
set -euo pipefail

SENSORS_SOCK="${HOMEPI_SENSORS_SOCKET:-/run/homepi/sensors/sensors.sock}"
BROKER_SOCK="${HOMEPI_BROKER_SOCKET:-/run/homepi/broker/broker.sock}"
BACKEND_URL="${HOMEPI_BACKEND_URL:-http://127.0.0.1:3000}"
EXPECTED_SENSORS="${HOMEPI_EXPECTED_SENSOR_COUNT:-38}"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

pass() {
  echo "OK: $*"
}

command -v nc >/dev/null 2>&1 || fail "nc is required"

if [[ ! -S "${SENSORS_SOCK}" ]]; then
  fail "sensors socket missing: ${SENSORS_SOCK}"
fi
pass "sensors socket present"

health_line="$(printf '%s\n' '{"v":1,"id":"verify-health","source":"verify-sensors","target":"homepi-sensors","command":"getHealth","correlationId":"verify"}' | nc -U -w 2 "${SENSORS_SOCK}" | head -n1)"
echo "${health_line}" | grep -q '"ok":true' || fail "getHealth did not return ok"
pass "facade getHealth responds"

snapshot_line="$(printf '%s\n' '{"v":1,"id":"verify-snapshot","source":"verify-sensors","target":"homepi-sensors","command":"sensors.snapshot","correlationId":"verify"}' | nc -U -w 2 "${SENSORS_SOCK}" | head -n1)"
echo "${snapshot_line}" | grep -q '"ok":true' || fail "sensors.snapshot did not return ok"
sensor_count="$(echo "${snapshot_line}" | sed -n 's/.*"sensorCount":\([0-9]*\).*/\1/p')"
if [[ -z "${sensor_count}" ]]; then
  fail "could not parse sensorCount from snapshot"
fi
if [[ "${sensor_count}" -lt "${EXPECTED_SENSORS}" ]]; then
  fail "expected at least ${EXPECTED_SENSORS} sensors, got ${sensor_count}"
fi
pass "snapshot contains ${sensor_count} sensors"

if [[ -S "${BROKER_SOCK}" ]]; then
  pass "broker socket present"
else
  echo "WARN: broker socket missing (${BROKER_SOCK})"
fi

if command -v curl >/dev/null 2>&1; then
  api_body="$(curl -fsS "${BACKEND_URL}/api/contact-sensors" 2>/dev/null || true)"
  if [[ -n "${api_body}" ]]; then
    echo "${api_body}" | grep -q '"sensorCount"' || fail "API snapshot missing sensorCount"
    pass "GET /api/contact-sensors shape valid"
  else
    echo "WARN: backend API not reachable at ${BACKEND_URL}"
  fi
fi

echo "Contact sensors operational verification passed."
