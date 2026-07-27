#!/usr/bin/env bash
# Installs HomePi Node platform services (broker, health, module facades).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

require_root
assert_node_version
assert_pnpm_version

install_unit() {
  local filter="$1"
  local service_path="$2"
  local unit_src="$3"
  local service_name
  service_name="$(basename "${service_path}")"
  local install_dir="/opt/homepi/services/${service_path}"
  local staging_dir
  staging_dir="$(mktemp -u "/tmp/homepi-${service_name}-deploy.XXXXXX")"

  echo "==> Installing ${service_name}"
  sudo -u homepi bash -c "
    cd \"${REPO_ROOT}\" &&
    pnpm --filter \"${filter}\" deploy --prod \"${staging_dir}\"
  "
  mkdir -p "${install_dir}"
  rsync -a --delete "${staging_dir}/" "${install_dir}/"
  rm -rf "${staging_dir}"
  chown -R homepi:homepi "${install_dir}"
  install -m 0644 "${unit_src}" "/etc/systemd/system/${service_name}.service"
  systemctl daemon-reload
  systemctl enable "${service_name}.service"
  systemctl reset-failed "${service_name}.service" 2>/dev/null || true
  systemctl restart "${service_name}.service" || true
}

install_unit "@homepi/service-broker" \
  "broker/homepi-broker" \
  "${REPO_ROOT}/services/broker/homepi-broker/systemd/homepi-broker.service"
install_unit "@homepi/service-health" \
  "health/homepi-health" \
  "${REPO_ROOT}/services/health/homepi-health/systemd/homepi-health.service"
install_unit "@homepi/service-audio" \
  "audio/homepi-audio" \
  "${REPO_ROOT}/services/audio/homepi-audio/systemd/homepi-audio.service"
install_unit "@homepi/service-sensors" \
  "sensors/homepi-sensors" \
  "${REPO_ROOT}/services/sensors/homepi-sensors/systemd/homepi-sensors.service"
install_unit "@homepi/service-homekit" \
  "homekit/homepi-homekit" \
  "${REPO_ROOT}/services/homekit/homepi-homekit/systemd/homepi-homekit.service"

echo "==> Restarting native services that bind under /run/homepi/audio (avoid stale sockets after RuntimeDirectory recreation)"
for unit in homepi-usb-devices homepi-pcm-router homepi-hifi-serial homepi-metadata homepi-audio-paging homepi-audio-orchestrator; do
  systemctl restart "${unit}.service" 2>/dev/null || true
done
systemctl restart homepi-backend.service 2>/dev/null || true

echo "Node platform services installed."
