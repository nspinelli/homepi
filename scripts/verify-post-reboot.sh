#!/usr/bin/env bash
# Post-reboot verification for HomePi operational stack.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

FAIL=0

check() {
  local label="$1"
  local result="$2"
  if [[ "${result}" == "ok" ]]; then
    echo "  OK  ${label}"
  else
    echo "  FAIL ${label}: ${result}"
    FAIL=1
  fi
}

warn() {
  echo "  WARN $*"
}

echo "=== Post-reboot HomePi verification ==="

ssh_unit=""
if systemctl list-unit-files ssh.service >/dev/null 2>&1; then
  ssh_unit="ssh.service"
elif systemctl list-unit-files sshd.service >/dev/null 2>&1; then
  ssh_unit="sshd.service"
fi

if [[ -n "${ssh_unit}" ]]; then
  if systemctl is-active "${ssh_unit}" >/dev/null 2>&1; then
    check "SSH (${ssh_unit})" "ok"
  else
    check "SSH (${ssh_unit})" "not active"
  fi
  if systemctl is-enabled "${ssh_unit}" >/dev/null 2>&1; then
    check "SSH enabled" "ok"
  else
    check "SSH enabled" "not enabled"
  fi
else
  warn "ssh/sshd unit not found"
fi

if [[ -r /proc/asound/cards ]]; then
  echo ""
  echo "=== ALSA cards ==="
  cat /proc/asound/cards
  if grep -qE 'HomePiZonesA|HomePiZonesB' /proc/asound/cards 2>/dev/null; then
    check "ALSA loopback zones" "ok"
  else
    warn "HomePi loopback cards not listed (may need modprobe or reboot)"
  fi
else
  warn "/proc/asound/cards not readable"
fi

if [[ -f /run/homepi/pending-reboot-reason ]]; then
  warn "Pending reboot was requested before last boot: $(cat /run/homepi/pending-reboot-reason)"
fi

echo ""
bash "${REPO_ROOT}/scripts/verify-operational.sh" || FAIL=1

if [[ "${FAIL}" -ne 0 ]]; then
  echo ""
  echo "Post-reboot verification failed." >&2
  exit 1
fi

echo ""
echo "Post-reboot verification passed."
