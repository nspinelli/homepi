#!/usr/bin/env bash
# Roll back HomePi production from v2 to v1: stop v2, restore legacy DB, reinstall v1 stack.
set -euo pipefail

V2_ROOT="/home/homepi/homepi-v2"
V1_ROOT="/home/homepi/homepi"
STATE_DIR="/opt/homepi/runtime/state"
BACKUP_DIR="/opt/homepi/runtime/backups"
TIMESTAMP="$(date -u +%Y%m%dT%H%M%SZ)"

log() { echo "==> $*"; }

require_sudo() {
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Re-run with sudo: sudo bash $0" >&2
    exit 1
  fi
}

stop_v2_stack() {
  log "Stopping v2 services"
  systemctl stop homepi-backend.service 2>/dev/null || true

  for i in $(seq 1 16); do
    systemctl stop "homepi-shairport@${i}.service" 2>/dev/null || true
    systemctl stop "homepi-metadata@${i}.service" 2>/dev/null || true
  done

  for unit in homepi-shairport homepi-audio-metadata homepi-pcm-router \
    homepi-hifi2-controller homepi-hifi-serial homepi-usb-devices \
    homepi-nqptp homepi-logging homepi-storage homepi-events; do
    systemctl stop "${unit}.service" 2>/dev/null || true
  done
  sleep 2
}

uninstall_v2_services() {
  log "Uninstalling v2 services"
  if [[ -d "${V2_ROOT}" ]]; then
    for script in \
      "${V2_ROOT}/services/homepi-shairport/scripts/uninstall.sh" \
      "${V2_ROOT}/services/homepi-audio-metadata/scripts/uninstall.sh" \
      "${V2_ROOT}/services/homepi-pcm-router/scripts/uninstall.sh" \
      "${V2_ROOT}/services/homepi-hifi2-controller/scripts/uninstall.sh" \
      "${V2_ROOT}/services/homepi-nqptp/scripts/uninstall.sh" \
      "${V2_ROOT}/services/homepi-usb-devices/scripts/uninstall.sh" \
      "${V2_ROOT}/core/logging/scripts/uninstall.sh" \
      "${V2_ROOT}/core/storage/scripts/uninstall.sh" \
      "${V2_ROOT}/core/events/scripts/uninstall.sh"; do
      if [[ -x "${script}" ]] || [[ -f "${script}" ]]; then
        bash "${script}" || true
      fi
    done
  fi

  systemctl disable homepi-backend.service 2>/dev/null || true
  rm -f /etc/systemd/system/homepi-backend.service
  rm -f /etc/systemd/system/homepi-audio-metadata.service
  rm -f /etc/systemd/system/homepi-hifi2-controller.service
  rm -f /etc/systemd/system/homepi-usb-post-assignment.service
  rm -f /etc/systemd/system/homepi-v2-*.service
  rm -f /etc/systemd/system/homepi-v2-mock.target
  systemctl daemon-reload
}

remove_v2_artifacts() {
  log "Removing v2 install artifacts"
  rm -rf /opt/homepi/services/backend
  rm -rf /opt/homepi/services/events
  rm -rf /opt/homepi/services/storage
  rm -rf /opt/homepi/services/logging
  rm -rf /opt/homepi/services/hifi2-controller
  rm -rf /opt/homepi/services/audio-metadata
  rm -rf /opt/homepi/apps/frontend/dist
  rm -rf /opt/homepi/contracts
  rm -rf /opt/homepi/v2
  rm -rf /run/homepi/events.sock /run/homepi/shairport 2>/dev/null || true
}

restore_legacy_database() {
  log "Restoring v1 sqlite database"
  mkdir -p "${BACKUP_DIR}"
  if [[ -f "${STATE_DIR}/homepi.sqlite" ]]; then
    cp -a "${STATE_DIR}/homepi.sqlite" "${BACKUP_DIR}/homepi.sqlite.v2-before-rollback.${TIMESTAMP}"
  fi

  local legacy_db=""
  for candidate in \
    "${STATE_DIR}/homepi.sqlite.legacy."* \
    "${BACKUP_DIR}/homepi.sqlite.pre-v2-"* \
    "${BACKUP_DIR}/homepi.sqlite.20260615T191117Z"; do
    if [[ -f "${candidate}" ]]; then
      legacy_db="${candidate}"
      break
    fi
  done

  if [[ -z "${legacy_db}" ]]; then
    echo "ERROR: no legacy sqlite backup found under ${STATE_DIR} or ${BACKUP_DIR}" >&2
    exit 1
  fi

  log "Using legacy database: ${legacy_db}"
  rm -f "${STATE_DIR}/homepi.sqlite-wal" "${STATE_DIR}/homepi.sqlite-shm"
  rm -f "${STATE_DIR}/homepi-v2.sqlite" "${STATE_DIR}/homepi.sqlite.v2-migrated"
  cp -a "${legacy_db}" "${STATE_DIR}/homepi.sqlite"
  chown homepi:homepi "${STATE_DIR}/homepi.sqlite"
  chmod 0644 "${STATE_DIR}/homepi.sqlite"
}

restore_v1_stack() {
  log "Installing v1 operational stack"
  systemctl enable mosquitto 2>/dev/null || true
  systemctl start mosquitto 2>/dev/null || true
  bash "${V1_ROOT}/scripts/install-operational.sh"
}

remove_v2_checkout() {
  log "Removing v2 source checkout"
  rm -rf "${V2_ROOT}"
}

verify_v1() {
  log "Verifying v1 stack"
  local failed=0
  for unit in nginx homepi-backend mosquitto homepi-usb-devices homepi-hifi-serial \
    homepi-pcm-router homepi-shairport-supervisor homepi-nqptp; do
    if systemctl is-active --quiet "${unit}.service" 2>/dev/null; then
      echo "  OK  ${unit}"
    else
      echo "  FAIL ${unit}" >&2
      failed=1
    fi
  done

  for unit in homepi-events homepi-storage homepi-logging homepi-hifi2-controller \
    homepi-shairport.service; do
    if systemctl is-active --quiet "${unit}.service" 2>/dev/null; then
      echo "  WARN v2 unit still active: ${unit}" >&2
      failed=1
    fi
  done

  if curl -sf "http://127.0.0.1/api/health" | grep -q '"ok"'; then
    echo "  OK  http://127.0.0.1/api/health"
  else
    echo "  FAIL v1 backend health" >&2
    failed=1
  fi

  [[ "${failed}" -eq 0 ]] || {
    journalctl -u homepi-backend -u homepi-pcm-router -u homepi-hifi-serial -n 40 --no-pager >&2
    exit 1
  }

  echo ""
  echo "Rollback complete. HomePi v1 is live at http://homepi.local/"
  echo "v2 database backup: ${BACKUP_DIR}/homepi.sqlite.v2-before-rollback.${TIMESTAMP}"
}

main() {
  require_sudo
  log "HomePi v2 → v1 production rollback"
  stop_v2_stack
  uninstall_v2_services
  remove_v2_artifacts
  restore_legacy_database
  restore_v1_stack
  remove_v2_checkout
  verify_v1
}

main "$@"
