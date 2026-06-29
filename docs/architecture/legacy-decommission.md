# HomePi Legacy Decommission Inventory

**Status:** Execute in Phase 10 — only after user confirms Phases 1–9 testing passed.

**Step-by-step checklist:** [`v2-migration-checklist.md`](v2-migration-checklist.md) (v1 → v2 migration with functional parity matrix).

## Services / systemd units

| Legacy unit | Replaced by | Phase 10 action |
|-------------|-------------|-----------------|
| `homepi-events.service` | `homepi-broker.service` | stop, disable, remove unit file |
| `homepi-audio-orchestrator.service` | `homepi-audio.service` | stop, disable, remove if fully absorbed |

## Flat socket paths

| Legacy path | New path | Phase 10 action |
|-------------|----------|-----------------|
| `/run/homepi/events.sock` | `/run/homepi/broker/broker.sock` | remove socket + symlink |
| `/run/homepi/usb-devices.sock` | `/run/homepi/usb/usb.sock` | remove socket + symlink |
| `/run/homepi/hifi-serial.sock` | `/run/homepi/audio/hifi-serial.sock` | remove socket + symlink |
| `/run/homepi/pcm-router.sock` | `/run/homepi/audio/pcm-router.sock` | remove socket + symlink |
| `/run/homepi/metadata.sock` | `/run/homepi/audio/metadata.sock` | remove socket + symlink |
| `/run/homepi/audio-realtime.sock` | `/run/homepi/audio/audio-realtime.sock` | remove socket + symlink |
| `/run/homepi/audio-paging.sock` | `/run/homepi/audio/paging.sock` | remove socket + symlink |

## Backend code to delete

| Path | Purpose |
|------|---------|
| `apps/backend/src/status/` | Legacy status aggregation |
| `apps/backend/src/events/events-broker-bridge.ts` | Legacy events.sock bridge |
| `apps/backend/src/usb-devices/usb-devices-event-bridge.ts` | Per-service bridge |
| `apps/backend/src/hifi-serial/hifi-serial-event-bridge.ts` | Per-service bridge |
| `apps/backend/src/pcm-router/pcm-router-event-bridge.ts` | Per-service bridge |
| `apps/backend/src/metadata/metadata-event-bridge.ts` | Per-service bridge |
| `apps/backend/src/audio/audio-realtime-bridge.ts` | Per-service bridge |
| `apps/backend/src/system-status-store.ts` | In-memory health store |
| `apps/backend/src/core-status-builder.ts` | Flat status builder |
| Direct internal socket clients in `index.ts` | Replaced by facade clients |

## Core artifacts

| Path | Phase 10 action |
|------|-----------------|
| `core/events/` runtime (`homepi-events` binary, systemd unit) | remove from install |
| Install hooks referencing `homepi-events` | remove |

## API fields

| Field | Phase 10 action |
|-------|-----------------|
| `GET /api/core/status` → `system` (flat snapshot) | remove deprecated field |

## Verification (negative checks)

```bash
test ! -S /run/homepi/events.sock
systemctl is-enabled homepi-events.service 2>/dev/null && exit 1 || true
test -S /run/homepi/health/health.sock
test -S /run/homepi/broker/broker.sock
test -S /run/homepi/audio/audio.sock
test -S /run/homepi/sensors/sensors.sock
```

## User sign-off checklist

- [ ] Both module facades operational on hardware
- [ ] Status page shows hierarchical health with icons
- [ ] Direct commands work with broker stopped
- [ ] Failure isolation scenarios verified
- [ ] Reboot persistence confirmed
