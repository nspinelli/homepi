# HomePi Event Flow

## Overview

Service status flows from native daemons into the backend, then outward to UI clients. Polling is not used as the normal path for service health.

```text
Native daemon
  -> Unix socket event stream (or journald lifecycle logs)
  -> Backend event bridge
  -> SystemStatusStore
  -> SSE / WebSocket
  -> Frontend dashboard
```

## Backend startup sequence

1. Load config
2. Create logger and `SystemStatusStore` (safe defaults, all services `offline`)
3. Create HTTP/SSE/WebSocket server
4. Start native event bridges with exponential backoff reconnect
5. Start journald log bridge (UI logs + lifecycle status for nqptp/metadata/shairport)
6. Load one-time startup snapshots (`getHealth` / `systemctl`) via `Promise.allSettled`
7. Start slow fallback reconciliation (120s interval)
8. Broadcast `system_status_delta` only when status fields change

## Client delivery

| Mechanism | Direction | Role |
|-----------|-----------|------|
| Unix socket subscribe | Native → Backend | Authoritative service events |
| `SystemStatusStore` | Backend internal | Authoritative dashboard cache |
| SSE `/events` | Backend → Browser | `system_status_snapshot` on connect, `system_status_delta` on change, `heartbeat` every 30s |
| WebSocket `/ws` | Backend → Browser | Initial snapshot; optional `delta` on status change |
| REST `/api/core/status` | Backend → Browser | On-demand snapshot (uptime computed at read time) |

SSE and WebSocket are **outbound only**. The backend does not consume its own SSE stream for service state.

## Uptime

`uptimeMs` is computed at read time from process `startedAt`:

```ts
uptimeMs: Math.max(0, Date.now() - startedAt.getTime())
```

No periodic uptime polling interval runs in the backend.

## CPU temperature

Reading `/sys/class/thermal/thermal_zone0/temp` every 5 seconds is allowed (hardware sysfs). Updates broadcast through the status coordinator when the value changes.
