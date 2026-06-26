# HomePi Service Status

## Event contract

Native services emit `EventEnvelope` messages (`core/events/schema/event-envelope.schema.json`) on topic `system.service`:

```json
{
  "version": 1,
  "source": "homepi-hifi-serial",
  "topic": "system.service",
  "event": "controller_connected",
  "correlationId": "startup",
  "timestamp": "2026-05-29T00:00:00.000Z",
  "payload": {
    "status": "healthy",
    "connected": true,
    "degraded": false,
    "syncInProgress": false
  }
}
```

Payload shape: `core/events/schema/service-health-payload.schema.json`.

Additional broker topics (e.g. `modules.metadata.*`, `audio.paging.*`) feed domain-specific UI state; service health for the dashboard primarily uses `system.service` and startup snapshots.

## Status mapping rules

### USB devices (`homepi-usb-devices`)

| Condition | Dashboard status |
|-----------|------------------|
| Socket unreachable | `offline` |
| Assignments degraded | `degraded` |
| Otherwise reachable | `healthy` |

Events: `service_started`, `device_added`, `device_removed`, `assignments_degraded`, `assignments_recovered`.

### HiFi serial (`homepi-hifi-serial`)

| Condition | Dashboard status |
|-----------|------------------|
| Controller disconnected | `offline` |
| Sync in progress | `degraded` |
| Service degraded | `degraded` |
| Controller connected normally | `healthy` |

Events: `service_started`, `hardware_connected`, `sync_started`, `sync_completed`, `controller_connected`, `service_degraded`, `service_recovered`.

### PCM router (`homepi-pcm-router`)

| Condition | Dashboard status |
|-----------|------------------|
| DAC open, audio active | `healthy` |
| DAC unassigned / unavailable | `degraded` |
| Bridge disconnected | `offline` |

Legacy `modules.pcm` `health` and `dac_state` events are also mapped.

### NQPTP (`homepi-nqptp`) — documented exception

No HomePi Unix socket API. Status from:

1. Journald `core.runtime` lifecycle logs
2. Slow fallback `systemctl is-active homepi-nqptp` (120s)

### Metadata (`homepi-metadata`)

Single daemon (legacy `homepi-metadata@N` template disabled on install). Status from:

1. Unix socket `metadata.sock` + journald lifecycle logs
2. Fallback: `systemctl is-active homepi-metadata` (120s)

Progress telemetry is **not** on the event bus; it flows via `audio-realtime.sock`.

### Shairport supervisor

Same fallback pattern as nqptp (journald + 120s systemd reconciliation).

### Audio orchestrator (`homepi-audio-orchestrator`)

Status from `core/events` lifecycle and orchestration topics; no dedicated dashboard socket.

### Audio paging (`homepi-audio-paging`)

Status from `audio-paging.sock` `getHealth` and broker `audio.paging.*` events when the service is installed.

## Startup snapshots

On backend start, each service is queried **once**:

- Socket `getHealth` for USB, HiFi, paging (when installed)
- PCM subscribe snapshot
- Metadata socket bootstrap where available
- `systemctl is-active` for nqptp, metadata, shairport

Failures mark the service `offline` and log `startup_snapshot_failed` without aborting the backend.

## Fallback reconciliation

Configure in backend `service-config.json`:

```json
{
  "status": {
    "fallbackReconciliation": {
      "enabled": true,
      "intervalMs": 120000
    }
  }
}
```

- Default interval: **120 seconds** when not specified
- Purpose: Correct stale state after missed events
- Logs `fallback_reconciliation_corrected` only when a value changes
- Not a substitute for normal event-driven updates

## Reconnect backoff

Socket event bridges use: `1s → 2s → 5s → 10s → 30s` max, reset on successful connect.

Structured log events: `event_bridge_connecting`, `event_bridge_connected`, `event_bridge_disconnected`, `event_bridge_reconnect_scheduled`, `event_bridge_reconnect_failed`.
