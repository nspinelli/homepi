#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

FAIL=0
WARN=0

status_line() {
  local svc="$1"
  local state
  state="$(systemctl is-active "${svc}" 2>/dev/null || echo unknown)"
  printf "  %-32s %s\n" "${svc}" "${state}"
  if [[ "${state}" != "active" ]]; then
    FAIL=1
  fi
}

warn_line() {
  echo "  WARN $*"
  WARN=1
}

check_http() {
  local label="$1"
  local url="$2"
  local code
  code=$(curl -sf -o /dev/null -w "%{http_code}" "${url}" 2>/dev/null || echo "000")
  if [[ "${code}" == "200" ]]; then
    echo "  OK  ${label} (${code})"
  else
    echo "  FAIL ${label} (${code}) ${url}"
    FAIL=1
  fi
}

echo "=== HomePi operational status ==="
for svc in nginx homepi-backend homepi-events avahi-daemon avahi-homepi-alias mosquitto \
  homepi-usb-devices homepi-nqptp homepi-pcm-router homepi-metadata \
  homepi-hifi-serial homepi-shairport-supervisor homepi-audio-orchestrator; do
  status_line "${svc}"
done

echo ""
echo "=== SSH ==="
ssh_unit=""
if systemctl list-unit-files ssh.service >/dev/null 2>&1; then
  ssh_unit="ssh.service"
elif systemctl list-unit-files sshd.service >/dev/null 2>&1; then
  ssh_unit="sshd.service"
fi
if [[ -n "${ssh_unit}" ]]; then
  printf "  %-32s %s\n" "${ssh_unit}" "$(systemctl is-active "${ssh_unit}" 2>/dev/null || echo unknown)"
  if ! systemctl is-enabled "${ssh_unit}" >/dev/null 2>&1; then
    echo "  FAIL ${ssh_unit} is not enabled"
    FAIL=1
  fi
else
  warn_line "ssh/sshd unit not found"
fi

echo ""
echo "=== Sudoers ==="
for f in /etc/sudoers.d/homepi-shairport /etc/sudoers.d/homepi-usb-post-assignment; do
  if sudo test -f "${f}"; then
    if sudo visudo -cf "${f}" >/dev/null 2>&1; then
      echo "  OK  ${f}"
    else
      echo "  FAIL ${f} failed visudo check"
      FAIL=1
    fi
  else
    warn_line "missing ${f}"
  fi
done

echo ""
echo "=== nqptp version ==="
NQPTP_CONFIG="/opt/homepi/services/nqptp/config/service-config.json"
if [[ -x /opt/homepi/services/nqptp/bin/nqptp ]] && [[ -f "${NQPTP_CONFIG}" ]]; then
  expected="$(read_json_field "${NQPTP_CONFIG}" "nqptp.upstreamVersion" 2>/dev/null || echo "")"
  actual="$(/opt/homepi/services/nqptp/bin/nqptp -V 2>&1 | head -1 || true)"
  if [[ -n "${expected}" ]] && [[ "${actual}" == *"${expected}"* ]]; then
    echo "  OK  nqptp ${actual} (expected ${expected})"
  else
    echo "  FAIL nqptp version mismatch: ${actual} (expected ${expected})"
    FAIL=1
  fi
else
  warn_line "nqptp binary or config missing"
fi

echo ""
echo "=== ALSA ==="
if lsmod | grep -q snd_aloop; then
  echo "  OK  snd_aloop module loaded"
else
  warn_line "snd_aloop not loaded"
fi
if [[ -r /proc/asound/cards ]]; then
  if grep -qE 'HomePiZonesA|HomePiZonesB' /proc/asound/cards 2>/dev/null; then
    echo "  OK  HomePi loopback cards present"
  else
    warn_line "HomePi loopback cards not listed in /proc/asound/cards"
  fi
else
  warn_line "/proc/asound/cards not readable"
fi

echo ""
echo "=== Mosquitto loopback ==="
if command -v mosquitto_pub >/dev/null 2>&1 && command -v mosquitto_sub >/dev/null 2>&1; then
  test_topic="homepi/install-verify/$$"
  ( sleep 0.3; mosquitto_pub -h 127.0.0.1 -t "${test_topic}" -m ok ) &
  pub_pid=$!
  if timeout 3 mosquitto_sub -h 127.0.0.1 -t "${test_topic}" -C 1 -W 2 >/dev/null; then
    wait "${pub_pid}" 2>/dev/null || true
    echo "  OK  mosquitto pub/sub loopback"
  else
    kill "${pub_pid}" 2>/dev/null || true
    echo "  FAIL mosquitto pub/sub loopback test"
    FAIL=1
  fi
else
  warn_line "mosquitto clients not installed"
fi

echo ""
echo "=== mDNS ==="
getent hosts homepi.local || warn_line "homepi.local not resolved"

echo ""
echo "=== HTTP checks ==="
check_http "frontend homepi.local" "http://homepi.local/"
check_http "api health" "http://homepi.local/api/health"
curl -sf http://homepi.local/api/health | head -c 120
echo ""

if [[ "${FAIL}" -ne 0 ]]; then
  echo ""
  echo "Operational verification failed." >&2
  exit 1
fi

if [[ "${WARN}" -ne 0 ]]; then
  echo ""
  echo "Operational verification passed with warnings."
  exit 0
fi

echo ""
echo "Operational verification passed."
