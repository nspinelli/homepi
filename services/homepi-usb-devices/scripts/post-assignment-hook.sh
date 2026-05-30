#!/usr/bin/env bash
# Deploys udev/modprobe artifacts and restarts dependent services after assignment save.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

log() { echo "==> $*"; }

PRIMARY_MODPROBE_CHANGED=false

deploy_rc=0
if bash "${SCRIPT_DIR}/deploy-audio-modprobe.sh"; then
  deploy_rc=0
else
  deploy_rc=$?
fi

if [[ "${deploy_rc}" -eq 0 ]]; then
  PRIMARY_MODPROBE_CHANGED=true
  log "Primary audio modprobe changed — applying stable ALSA name"
  if bash "${SCRIPT_DIR}/apply-primary-audio-alsa.sh"; then
    log "HomePiPrimary ALSA name is active"
  else
    log "Scheduling reboot to apply HomePiPrimary ALSA name"
    sync
    systemctl reboot --no-wall
    exit 0
  fi
elif [[ "${deploy_rc}" -ne 2 ]]; then
  log "WARN: primary audio modprobe deploy failed (check Primary Audio Output assignment)"
fi

if ! bash "${SCRIPT_DIR}/deploy-udev-rules.sh"; then
  log "WARN: serial udev deploy skipped (assign Primary Serial or check /dev/vHifi)"
fi

if systemctl is-enabled homepi-hifi-serial.service >/dev/null 2>&1; then
  log "Restarting homepi-hifi-serial"
  systemctl restart homepi-hifi-serial.service
fi

if systemctl is-enabled homepi-pcm-router.service >/dev/null 2>&1; then
  log "Restarting homepi-pcm-router"
  systemctl restart homepi-pcm-router.service
fi
