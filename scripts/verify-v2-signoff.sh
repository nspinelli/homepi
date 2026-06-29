#!/usr/bin/env bash
# Phase E v2 architecture sign-off: negative checks, API shape, failure isolation.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
API_BASE="${HOMEPI_API_BASE:-http://127.0.0.1:3000}"
FAIL=0

fail() {
  echo "  FAIL $*"
  FAIL=1
}

pass() {
  echo "  OK  $*"
}

warn() {
  echo "  WARN $*"
}

restore_services() {
  sudo systemctl start homepi-broker.service 2>/dev/null || true
  sudo systemctl start homepi-health.service 2>/dev/null || true
  sudo systemctl restart homepi-backend.service 2>/dev/null || true
}

trap restore_services EXIT

echo "=== Phase E: v2 negative checks ==="
if [[ -S /run/homepi/events.sock ]]; then
  fail "legacy /run/homepi/events.sock still present"
else
  pass "/run/homepi/events.sock absent"
fi

if systemctl is-enabled homepi-events.service >/dev/null 2>&1; then
  fail "homepi-events.service still enabled"
else
  pass "homepi-events.service not enabled"
fi

for sock in \
  /run/homepi/broker/broker.sock \
  /run/homepi/health/health.sock \
  /run/homepi/audio/audio.sock \
  /run/homepi/sensors/sensors.sock \
  /run/homepi/audio/hifi-serial.sock \
  /run/homepi/usb/usb.sock; do
  if [[ -S "${sock}" ]]; then
    pass "${sock}"
  else
    fail "missing ${sock}"
  fi
done

for legacy in usb-devices.sock hifi-serial.sock pcm-router.sock metadata.sock audio-paging.sock; do
  if [[ -e "/run/homepi/${legacy}" ]]; then
    fail "legacy flat path still present: /run/homepi/${legacy}"
  else
    pass "no /run/homepi/${legacy}"
  fi
done

echo ""
echo "=== Phase E: operational verification ==="
if ! bash "${REPO_ROOT}/scripts/verify-operational.sh"; then
  echo "  WARN verify-operational failed — restarting native audio/USB services and retrying once"
  sudo systemctl restart \
    homepi-usb-devices.service \
    homepi-pcm-router.service \
    homepi-hifi-serial.service \
    homepi-metadata.service \
    homepi-audio-paging.service \
    homepi-audio-orchestrator.service \
    homepi-backend.service 2>/dev/null || true
  sleep 3
  if bash "${REPO_ROOT}/scripts/verify-operational.sh"; then
    pass "verify-operational.sh (after native service restart)"
  else
    fail "verify-operational.sh"
  fi
else
  pass "verify-operational.sh"
fi

echo ""
echo "=== Phase E: core status API shape ==="
CORE_JSON="$(curl -sf "${API_BASE}/api/core/status" || true)"
if [[ -z "${CORE_JSON}" ]]; then
  fail "GET /api/core/status unreachable"
else
  python3 - "${CORE_JSON}" <<'PY'
import json, sys
fail = False
payload = json.loads(sys.argv[1])
data = payload.get("data") or {}
if payload.get("ok") is not True:
    print("  FAIL core status ok!=true"); fail = True
if data.get("healthServiceReachable") is not True:
    print("  FAIL healthServiceReachable!=true"); fail = True
if "system" in data:
    print("  FAIL deprecated system field still present"); fail = True
modules = data.get("modules") or []
if len(modules) < 1:
    print("  FAIL no modules in core status"); fail = True
else:
    ids = {m.get("module") for m in modules}
    for expected in ("audio", "contact-sensors"):
        if expected in ids:
            print(f"  OK  module present: {expected}")
        else:
            print(f"  FAIL missing module: {expected}"); fail = True
platform = data.get("platform") or []
if len(platform) < 1:
    print("  FAIL no platform entries"); fail = True
else:
    print(f"  OK  platform entries: {len(platform)}")
host = data.get("host") or {}
for key in ("uptimeMs", "cpuTempC", "lastEventAt"):
    if key not in host:
        print(f"  FAIL host missing {key}"); fail = True
if not fail:
    print("  OK  core status payload shape")
sys.exit(1 if fail else 0)
PY
  if [[ $? -ne 0 ]]; then FAIL=1; fi
fi

HEALTH_CODE="$(curl -sf -o /dev/null -w '%{http_code}' "${API_BASE}/api/health" || echo 000)"
if [[ "${HEALTH_CODE}" == "200" ]]; then
  pass "GET /api/health returns 200"
else
  fail "GET /api/health returned ${HEALTH_CODE}"
fi

echo ""
echo "=== Phase E: direct Hi-Fi socket (zone control path) ==="
HIFI_HEALTH="$(printf '{"method":"getHealth","correlationId":"phase-e"}\n' | timeout 3 nc -U /run/homepi/audio/hifi-serial.sock 2>/dev/null | head -1 || true)"
if [[ "${HIFI_HEALTH}" == *'"ok":true'* ]]; then
  pass "hifi-serial getHealth via canonical socket"
else
  fail "hifi-serial getHealth failed"
fi

echo ""
echo "=== Phase E: broker failure isolation ==="
sudo systemctl stop homepi-broker.service
sleep 1
if [[ -S /run/homepi/broker/broker.sock ]]; then
  warn "broker socket still present after stop"
fi
HIFI_AFTER_BROKER="$(printf '{"method":"getHealth","correlationId":"phase-e-broker-down"}\n' | timeout 3 nc -U /run/homepi/audio/hifi-serial.sock 2>/dev/null | head -1 || true)"
if [[ "${HIFI_AFTER_BROKER}" == *'"ok":true'* ]]; then
  pass "direct hifi-serial still healthy with broker stopped"
else
  fail "direct hifi-serial unhealthy with broker stopped"
fi
sudo systemctl start homepi-broker.service
sleep 2
if systemctl is-active --quiet homepi-broker.service; then
  pass "homepi-broker restarted"
else
  fail "homepi-broker failed to restart"
fi
sudo systemctl restart homepi-backend.service
sleep 2

echo ""
echo "=== Phase E: health observer failure isolation ==="
sudo systemctl stop homepi-health.service
sleep 1
CORE_DOWN="$(curl -s "${API_BASE}/api/core/status" 2>/dev/null || echo '{}')"
HEALTH_DOWN_CODE="$(curl -s -o /dev/null -w '%{http_code}' "${API_BASE}/api/health" 2>/dev/null || echo 000)"
REACHABLE="$(python3 -c "import json,sys; d=json.loads(sys.argv[1]); print(d.get('data',{}).get('healthServiceReachable'))" "${CORE_DOWN}" 2>/dev/null || echo unknown)"
if [[ "${REACHABLE}" == "False" || "${REACHABLE}" == "false" ]]; then
  pass "core status reports healthServiceReachable=false"
else
  fail "core status healthServiceReachable=${REACHABLE} (expected false)"
fi
if [[ "${HEALTH_DOWN_CODE}" == "503" ]]; then
  pass "GET /api/health returns 503 when health observer down"
else
  fail "GET /api/health returned ${HEALTH_DOWN_CODE} (expected 503)"
fi
sudo systemctl start homepi-health.service
sleep 2
if systemctl is-active --quiet homepi-health.service; then
  pass "homepi-health restarted"
else
  fail "homepi-health failed to restart"
fi

echo ""
echo "=== Phase E: manual parity reminders ==="
warn "Manual sign-off still required: AirPlay now-playing, volume slider, paging, reboot smoke test, 48h daily use"

echo ""
if [[ "${FAIL}" -eq 0 ]]; then
  echo "Phase E automated sign-off passed."
  exit 0
fi

echo "Phase E automated sign-off failed."
exit 1
