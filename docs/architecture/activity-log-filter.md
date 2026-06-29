# HomePi Activity Log Filter Contract

Events on the system status page must show **meaningful activity** — not transport noise.

## `uiVisible` field

Broker and backend events may include optional `uiVisible: boolean`:

- `false` — transport-only; never shown in the status page log
- `true` or omitted — eligible for display (subject to level filter)

## Excluded events (uiVisible: false)

| Event / topic | Reason |
|---------------|--------|
| `heartbeat` | SSE transport liveness |
| WebSocket `ping` / `pong` | WS transport liveness |
| `system_status_snapshot` | Reflected in module health UI |
| `system_status_delta` | Reflected in module health UI |

## Included events

| Source | Examples |
|--------|----------|
| `log_record` | Structured service logs |
| `homepi.audio.*` | Zone changes, capability changes |
| `homepi.sensors.*` | Contact, tamper, fault changes |
| `homepi.health.service.changed` | Service health transitions |
| Failures | Any event with error/degraded severity or `userMessage` in payload |

## Default UI filter

Hide `debug` severity. User may opt in via level filter on `/status`.

## Implementation

- Backend sets `uiVisible: false` on transport events in `event-broadcaster.ts`
- Frontend `buildLogEntries()` skips events where `uiVisible === false` or event name is in the exclusion list
