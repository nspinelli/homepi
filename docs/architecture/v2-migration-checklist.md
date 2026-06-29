# HomePi v1 → v2 Migration Checklist

Execute this checklist to **remove Architecture v1 (legacy)** and run **Architecture v2 (target)** only, while preserving all current system behavior (AirPlay, zone control, metadata, paging, status UI, live updates).

**Related docs:** [`legacy-decommission.md`](legacy-decommission.md) (inventory), [`homepi-service-isolation.md`](homepi-service-isolation.md), [`sockets-and-broker.md`](sockets-and-broker.md), [`service-health.md`](service-health.md).

---

## Terminology

| Term | Meaning |
|------|---------|
| **Architecture v1 (legacy)** | C++ `homepi-events` broker on `/run/homepi/events.sock`, flat command sockets (`/run/homepi/hifi-serial.sock`, etc.), backend per-service event bridges, legacy `register`/`subscribe` broker protocol |
| **Architecture v2 (target)** | Node `homepi-broker` on `/run/homepi/broker/broker.sock`, `homepi-health` observer, module facades (`homepi-audio`, `homepi-sensors`), canonical socket layout under `/run/homepi/{broker,health,audio,sensors,usb}/`, `@homepi/core-messaging` NDJSON command protocol |
| **Hybrid (historical)** | v2 platform services installed **and** v1 paths still on the critical path — **resolved** |

**Do not run Phase 10 cleanup until every step in Phases A–D passes** on hardware.

---

## Pre-flight: confirm hybrid baseline

Run on the Pi before starting migration work:

```bash
# v2 platform (should be active)
systemctl is-active homepi-broker homepi-health homepi-audio homepi-sensors homepi-backend

# v1 legacy (still present today — must be zero after migration)
systemctl is-active homepi-events 2>/dev/null || echo "homepi-events not running"
test -S /run/homepi/events.sock && echo "LEGACY events.sock present" || echo "events.sock absent"

# Flat legacy command sockets (must move to canonical paths)
ls -l /run/homepi/hifi-serial.sock /run/homepi/metadata.sock /run/homepi/pcm-router.sock 2>/dev/null

# v2 canonical sockets
ls -l /run/homepi/broker/broker.sock /run/homepi/health/health.sock /run/homepi/audio/audio.sock
```

Capture a **functional baseline** (see [Functional parity matrix](#functional-parity-matrix)) while hybrid is working.

---

## Phase A — Event fanout migration (highest risk)

**Goal:** All native publishers and backend SSE use **`homepi-broker`** only. `homepi-events` becomes unused.

### A1. Native service event publishers

Each service currently uses `HOMEPI_EVENTS_SOCKET=/run/homepi/events.sock`. Repoint to v2 broker publish API or add a compatibility shim.

| Service | Config / env | v2 action | Verify |
|---------|--------------|-----------|--------|
| `homepi-metadata` | `HOMEPI_EVENTS_SOCKET` | Publish via v2 broker `publish` command or shared C++ client | Metadata appears in UI after track change |
| `homepi-pcm-router` | `HOMEPI_EVENTS_SOCKET` | Same | PCM owner/routing updates in UI |
| `homepi-hifi-serial` | `service-config.json` → `eventsSocket` | Same | Zone power/volume events in activity log |
| `homepi-audio-paging` | `service-config.json` | Same | Paging events reach UI |
| Shairport hooks | `HOMEPI_EVENTS_SOCKET` in generated env | Same | AirPlay session events |
| `homepi-audio-orchestrator` | `service-config.json` | Same or decommission if absorbed by `homepi-audio` | Orchestration events |

**Implementation options (pick one, document in PR):**

1. **Implemented:** Extended `core/events` C++ client (`broker-protocol.cpp`) to speak v2 broker `publish`/`subscribe` over `/run/homepi/broker/broker.sock`

- [x] C++ `EventsClient` publishes/subscribes via v2 broker protocol when socket path is canonical
- [x] Service configs and defaults repointed to `/run/homepi/broker/broker.sock`
- [x] Deployed `.env` overrides updated on Pi (metadata, pcm-router, hifi-serial)
- [x] With `homepi-events` **stopped**, metadata events reach SSE via v2 broker
- [x] AirPlay playback audible with zone power + PCM routing (hardware verified)
- [ ] Full paging / USB parity matrix signed off on hardware

### A2. Backend broker bridge

| Task | File(s) | Done |
|------|---------|------|
| Point SSE broker bridge at `broker.sock` only | `apps/backend/src/index.ts`, `http-server.ts` | [x] |
| Replace legacy `register`/`subscribe` with v1 `command: subscribe` | `apps/backend/src/events/events-broker-bridge.ts` | [x] |
| Handle broker wire format `{ type: "event", event: … }` | `events-broker-bridge.ts`, `broker-event-adapter.ts` | [x] |
| Keep `adaptBrokerEnvelopeForUi()` for topic → legacy SSE event names | `audio-ui-bridge.ts` | [x] |
| Remove `eventsBrokerSocketPath` fallback to `events.sock` | `http-server.ts` | [x] |

- [x] `journalctl -u homepi-backend` shows broker bridge connected to **broker.sock**
- [x] SSE stream includes `metadata_snapshot` with `homepi-events` **stopped**

### A3. Broker-only SSE mode

| Task | Done |
|------|------|
| Confirm `HOMEPI_BROKER_ONLY_AUDIO_SSE` defaults true | [x] |
| Remove or gate per-service bridges behind feature flag (then delete) | [x] |

Services to retire after A2 verified:

- [x] `MetadataEventBridge` (gated off when broker-only SSE enabled)
- [x] `PcmRouterEventBridge` (gated off when broker-only SSE enabled)
- [x] `HifiSerialEventBridge` (kept — zone volume/power events still use direct socket until hifi publishes to broker)
- [ ] `UsbDevicesEventBridge` (kept — USB health still uses direct socket until broker publish added)

**Keep until parity proven:** `AudioRealtimeBridge` (progress socket may stay direct per spec).

**Verified:** With `homepi-events` stopped, backend connects only to `broker.sock` + `usb-devices.sock` for SSE; audio snapshot and `/events` stream remain healthy.

---

## Phase B — Command socket layout (canonical paths)

**Goal:** Internal services listen on canonical paths; facades and health probe those paths (no `legacySocket` fallback required).

| Legacy path (v1) | Canonical path (v2) | Owner | Done |
|------------------|----------------------|-------|------|
| `/run/homepi/events.sock` | `/run/homepi/broker/broker.sock` | broker | [x] |
| `/run/homepi/hifi-serial.sock` | `/run/homepi/audio/hifi-serial.sock` | hifi-serial | [x] |
| `/run/homepi/pcm-router.sock` | `/run/homepi/audio/pcm-router.sock` | pcm-router | [x] |
| `/run/homepi/metadata.sock` | `/run/homepi/audio/metadata.sock` | metadata | [x] |
| `/run/homepi/audio-realtime.sock` | `/run/homepi/audio/audio-realtime.sock` | metadata | [x] |
| `/run/homepi/audio-paging.sock` | `/run/homepi/audio/paging.sock` | audio-paging | [x] |
| `/run/homepi/usb-devices.sock` | `/run/homepi/usb/usb.sock` | usb-devices | [x] |

### B1. Native service systemd / config

- [x] Update each service `RuntimeDirectory`, socket bind path, and env examples
- [x] Install script creates canonical directories (`RuntimeDirectory=homepi/audio`, etc.)
- [x] Optional transitional symlinks legacy → canonical (remove in Phase E)

### B2. Registry

- [x] `core/service-registry/registry.json`: `commandSocket` = canonical path; remove or empty `legacySocket` entries
- [x] Remove `homepi-events` from `services[]` after A complete

### B3. Backend direct clients

| Client | Change | Done |
|--------|--------|------|
| `HifiSerialClient` | Route via `homepi-audio` facade or canonical socket | [x] |
| `PcmRouterClient` | Same | [x] |
| `MetadataClient` | Same | [x] |
| `UsbDevicesClient` | Canonical usb socket | [x] |
| `PagingClient` | Canonical paging socket | [x] |

- [x] `homepi-audio` facade stops needing `legacySocket` fallback in `audio-internal-proxy.ts`

### B4. Health probes

- [x] `homepi-health` probes only `commandSocket` (canonical); remove legacy fallback once paths exist
- [x] Status page shows **healthy** for hifi-serial, metadata, pcm-router when services running

**Verified on Pi (2026-06-28):** All canonical sockets present; legacy flat paths symlinked; backend bridges connect to canonical paths; `verify-operational.sh` canonical socket checks pass.

---

## Phase C — Backend simplification

**Goal:** Single health path via `homepi-health`; no duplicate flat status aggregation.

| Task | File / area | Done |
|------|-------------|------|
| Remove `SystemStatusStore` flat service fields where replaced by core status | `system-status-store.ts`, bridges | [x] |
| Remove `FallbackReconciliation` 120s socket polling | `status/fallback-reconciliation.ts` | [x] |
| Remove startup snapshot systemd polling for per-service status | `status/startup-snapshots.ts` | [x] |
| Remove journal-based service status inference (optional; keep log bridge) | `logging/journal-log-bridge.ts` | [x] |
| Drop deprecated `system` field from `GET /api/core/status` | `core-status-builder.ts`, frontend types | [x] |
| Keep `GET /api/health` as thin proxy of health snapshot (for monitors/load balancers) | `http-server.ts` | [x] |
| WebSocket status: source from core status / health only | `ws-handler.ts` | [x] |

- [x] No imports of deleted bridges in `http-server.ts`
- [x] `pnpm --filter @homepi/app-backend run test` passes
- [x] Frontend status header still surfaces errors (zero silent failures)

**Verified:** `GET /api/core/status` returns `modules`/`platform`/`services` from `homepi-health` plus `host` metrics only; SSE/WS broadcast host metric deltas; audio snapshot derives `services.*` from health observer.

---

## Phase D — Decommission v1 runtime

**Only after Phases A–C pass on hardware.**

### D1. Run cleanup script (review first)

```bash
sudo bash scripts/cleanup-legacy-architecture.sh
```

- [x] `homepi-events.service` disabled and unit removed
- [x] Flat socket symlinks removed
- [x] `core/events/` removed from install/uninstall paths

### D2. Remove from install/verify/docs

- [x] `scripts/uninstall-operational.sh` — no `core/events` uninstall hook (or guarded)
- [x] `scripts/scripts-install-readme.md`, `fresh-pi-runbook.md` — v2 only
- [x] `verify-operational.sh` legacy negative checks **pass**
- [x] `services/native/services-native-readme.md` — canonical paths only

### D3. Delete dead code (see [`legacy-decommission.md`](legacy-decommission.md))

- [x] `core/events/` runtime removed from install/uninstall (source retained for schemas)
- [ ] Backend per-service bridges — **kept** (still forward domain events to SSE; health from `homepi-health`)
- [ ] `homepi-audio-orchestrator` — **kept** (still required for AirPlay/PCM lifecycle; not absorbed by facade)

### D4. Registry cleanup

- [x] Remove `homepi-events` service entry
- [ ] Mark `contact-sensors` module `planned: false` when GPIO/HomeKit implemented

**Verified on Pi (2026-06-29):** `verify-operational.sh` exits 0; `events.sock` absent; `homepi-events` not enabled; all canonical sockets present.

---

## Phase E — Verification & sign-off

### Automated checks

```bash
bash scripts/verify-operational.sh          # must exit 0 including legacy negative checks
bash scripts/verify-v2-signoff.sh           # Phase E: API shape + failure isolation
pnpm run test                               # monorepo
curl -sf http://homepi.local/api/core/status | jq '.data.healthServiceReachable'
curl -sf -o /dev/null -w '%{http_code}\n' http://homepi.local/api/health   # 200
```

**Verified (2026-06-29):** `verify-v2-signoff.sh` passes on Pi — negative checks, operational verify, core status shape, direct hifi socket, broker/health failure isolation.

### Negative checks (v1 must be gone)

```bash
test ! -S /run/homepi/events.sock
! systemctl is-enabled homepi-events.service 2>/dev/null
test -S /run/homepi/broker/broker.sock
test -S /run/homepi/health/health.sock
test -S /run/homepi/audio/audio.sock
```

### Functional parity matrix

Test each row **before** (hybrid) and **after** (v2-only). All must match.

| Capability | How to test | Pass |
|------------|-------------|------|
| AirPlay playback | Stream from phone to a zone; audio audible | [x] |
| Now-playing header | Title/artist update within one track change | [ ] |
| Progress bar | Position advances; seek/skip updates | [ ] |
| Zone power | Toggle zone from UI | [ ] |
| Zone volume | Slider changes volume | [ ] |
| PCM routing indicator | Correct zone shows streaming indicator | [x] |
| Paging | Trigger page; chime + speech | [ ] |
| USB device assignment | USB routing still works if used | [ ] |
| Status page modules | Home Audio + Contact Sensors sections with icons/pills | [x] |
| Platform section | backend, health, broker, usb show correct state | [x] |
| Activity log | Meaningful events only (no heartbeats, no audio.realtime) | [ ] |
| SSE / WS | Transport cards show connected | [ ] |
| Broker stopped | Direct commands still work (zone power); UI shows degraded live updates | [x] |
| Health stopped | `/api/core/status` shows `healthServiceReachable: false`; modules still run | [x] |
| Reboot | All v2 services active; no v1 sockets; smoke test above | [ ] |

### User sign-off

- [ ] Home Audio module healthy under normal operation
- [ ] Contact Sensors module acceptable (planned/offline OK until hardware ready)
- [ ] No regressions reported for 48h daily use
- [ ] **`legacy-decommission.md` checklist complete**

---

## Rollback plan

If migration fails mid-phase:

1. Re-enable `homepi-events`: `sudo systemctl enable --now homepi-events`
2. Restore backend `eventsBrokerSocketPath` to `/run/homepi/events.sock`
3. Re-enable per-service bridges if disabled
4. Restore flat socket symlinks from backup (`/var/backups/homepi/install-*`)
5. `sudo systemctl restart homepi-backend`

Document the failing phase and parity matrix row before retrying.

---

## Suggested implementation order (summary)

```text
A1 Native publishers → v2 broker
A2 Backend v2 broker bridge
A3 Remove per-service SSE bridges
B1–B3 Canonical sockets + clients
B4 Health probes canonical-only
C   Backend status simplification
D   cleanup-legacy-architecture.sh + code deletion
E   Full parity matrix + sign-off
```

**Estimated risk:** Phase A is the largest effort (C++ event client or shim). Phases B–D are mostly path and deletion work once events flow through v2 broker.

---

## Current hybrid gaps (as of last audit)

Migration Phases A–E automated work is **complete**. Remaining items are manual parity / soak test:

- [ ] Reboot persistence (`bash scripts/verify-post-reboot.sh`)
- [ ] UI parity: now-playing, volume slider, paging, activity log filtering
- [ ] 48h daily-use soak with no regressions
