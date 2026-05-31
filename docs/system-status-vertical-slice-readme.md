# HomePi System Status Vertical Slice

## Purpose

This vertical slice proves the HomePi core platform works end-to-end before real feature modules are added. It connects:

`core/config` → `core/logging` → `core/runtime` → `core/health` → `core/events` → `core/transport` → `core/state` → `core/api` → **backend** → **SSE / WebSocket / REST** → **frontend dashboard**

No audio, sensors, HiFi2, Shairport-Sync, or module-specific logic is included.

---

## Endpoints

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/health` | Platform health report (`core/health` + `core/api`) |
| `GET` | `/api/runtime/status` | Backend runtime status (`core/runtime` schema) |
| `GET` | `/api/core/status` | Aggregated core service status + system snapshot |
| `GET` | `/events` | SSE live system status stream (`core/events`) |
| `GET` | `/ws` | WebSocket shell (`core/transport`) |

Legacy aliases `/api/events` and `/api/ws` remain available for older clients.

---

## SSE Event Contract

SSE frames use the documented HomePi **event envelope** (`core/events/schema/event-envelope.schema.json`).

```
event: system_status_snapshot
data: {"version":1,"id":"...","source":"homepi-backend","topic":"system.status",...}

event: system_status_delta
data: {"version":1,"event":"system_status_delta","payload":{"status":{...},"emittedAt":"..."}}

event: heartbeat
data: {"version":1,"event":"heartbeat","payload":{"kind":"heartbeat"}}
```

Behavior:

1. Initial `system_status_snapshot` after connect
2. `system_status_delta` when native service state changes (event-driven)
3. `heartbeat` events (30s transport liveness)
4. Connect/disconnect logged via `core/logging`

Example: `apps/backend/examples/sse-system-status-event.example.json`

---

## WebSocket Shell Contract

WebSocket messages are **NDJSON transport envelopes** (`core/transport/schema/transport-envelope.schema.json`).

1. Server sends `type: "snapshot"` with `payload.snapshot` on connect
2. Client may send `{ "type": "ping" }`
3. Server replies with `type: "response"`, `topic: "transport.ping"`, `payload.action: "pong"`

Example: `apps/backend/examples/ws-ping-pong.example.json`

Module command handling is intentionally not implemented.

---

## Frontend Dashboard Behavior

The React dashboard (`apps/frontend`):

1. Calls `GET /api/health` and `GET /api/core/status`
2. Connects to `/events` (SSE)
3. Connects to `/ws` (WebSocket, periodic ping)
4. Displays backend health, core service statuses, uptime, last event timestamp, SSE/WS connection state, and the last received event envelope

Configuration uses Vite env vars (`VITE_API_BASE_URL`, `VITE_EVENTS_URL`, `VITE_WS_URL`) with same-origin defaults in development.

---

## How Core Modules Are Used

| Core module | Usage in slice |
|-------------|----------------|
| `core/config` | Loads `apps/backend/config/service-config.json`; no direct `process.env` in apps |
| `core/logging` | All backend lifecycle and connection logs |
| `core/runtime` | Runtime status payload shape for `/api/runtime/status` |
| `core/health` | Health report for `/api/health` |
| `core/events` | SSE event envelopes |
| `core/transport` | WebSocket NDJSON envelopes, ping/pong |
| `core/state` | In-memory authoritative `system.status` snapshot |
| `core/api` | Success/error envelopes with `correlationId` |

---

## Startup Flow

1. Backend loads config via `@homepi/core-config`
2. Logger is created via `@homepi/core-logging`
3. `SystemStatusStore` initializes core readiness snapshot
4. HTTP server binds `127.0.0.1:3000`
5. Native event bridges start (HiFi, PCM, USB) with reconnect backoff
6. Journald bridge starts (UI logs + lifecycle status for services without sockets)
7. One-time startup snapshots load service health into the store
8. Slow fallback reconciliation starts (120s; fault recovery only)
9. SSE broadcaster emits `system_status_delta` on status **changes** (not on a fixed interval)
10. Frontend dev server proxies `/api`, `/events`, `/ws` to backend

See [architecture/event-flow.md](architecture/event-flow.md) for the full event-driven model.

Production uses NGINX (`infra/nginx/templates/homepi.nginx.conf.template`) to serve the frontend and proxy API, SSE, and WebSocket routes.

---

## Example API Responses

Health:

```json
{
  "ok": true,
  "correlationId": "req-001",
  "timestamp": "2026-05-27T16:00:00.000Z",
  "data": {
    "service": "homepi-backend",
    "status": "healthy",
    "checkedAt": "2026-05-27T16:00:00.000Z",
    "checks": [{ "name": "http", "status": "pass", "message": "Backend listening" }]
  }
}
```

Core status: `apps/backend/examples/core-status-response.example.json`  
Runtime status: `apps/backend/examples/runtime-status-response.example.json`

---

## Testing Instructions

```bash
# Install workspace dependencies
pnpm install

# Build all packages
pnpm build

# Typecheck
pnpm typecheck

# Run tests
pnpm test

# Run backend
pnpm --filter @homepi/app-backend start

# Run frontend (separate terminal)
pnpm --filter @homepi/app-frontend dev
```

Open `http://127.0.0.1:5173` during development. Confirm dashboard health, SSE connected, WebSocket connected, and live events.

---

## Intentionally Not Included

- Audio, sensors, HiFi2, Shairport-Sync, or other modules
- Module command routing over WebSocket
- Persistent state sync beyond in-memory `core/state`
- Authentication enforcement (contracts exist in `core/auth`)
- systemd process supervision (runtime helpers only)
