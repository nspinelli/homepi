#!/usr/bin/env bash
# Verifies the nqptp multi-zone patch applies cleanly against the pinned upstream tag.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/lib/install-common.sh
source "${REPO_ROOT}/scripts/lib/install-common.sh"

SERVICE_ROOT="${REPO_ROOT}/services/homepi-nqptp"
CONFIG_JSON="${SERVICE_ROOT}/config/service-config.json"
PATCH_FILE="${SERVICE_ROOT}/patches/multi-zone-play-client-refcount.patch"
UPSTREAM_REPO="https://github.com/mikebrady/nqptp.git"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

UPSTREAM_VERSION="$(read_json_field "${CONFIG_JSON}" "nqptp.upstreamVersion")" \
  || die "Could not read upstream version from ${CONFIG_JSON}"

log "Cloning nqptp ${UPSTREAM_VERSION} into ${WORK_DIR}"
git clone --depth 1 --branch "${UPSTREAM_VERSION}" "${UPSTREAM_REPO}" "${WORK_DIR}/nqptp-src"

assert_patch_applies "${PATCH_FILE}" "${WORK_DIR}/nqptp-src"
echo "OK  nqptp patch applies cleanly to upstream ${UPSTREAM_VERSION}"
