# HomePi Service Health

## Layered health model

Every service reports three layers where applicable:

| Layer | Meaning |
|-------|---------|
| `process` | systemd unit state |
| `readiness` | socket available, service accepting commands |
| `domain` | feature actually usable (device present, controller responding) |

Do not treat `systemd active` as equivalent to feature availability.

## Health observer

`homepi-health` owns systemd/D-Bus observation, registry loading, socket checks, and domain probes via `getHealth` / `getSnapshot`.

Socket: `/run/homepi/health/health.sock`

Commands:

- `health.snapshot` — full system snapshot
- `health.module.get` — single client module
- `health.service.get` — single service with layered status
- `ping`

## Schema

System snapshot: [`core/health/schema/health-snapshot.schema.json`](../../core/health/schema/health-snapshot.schema.json)

Example service entry:

```json
{
  "service": "homepi-pcm-router",
  "process": "active",
  "readiness": "not_ready",
  "domain": "missing_device",
  "userMessage": "Playback routing is unavailable because the primary audio DAC is not connected."
}
```

## Module rollup

Client modules (`audio`, `contact-sensors`) aggregate capability health from internal services. Each module includes `displayName`, `icon`, and `capabilities[]` for the status UI.

## C++ native services

Native daemons may continue using legacy `{method, correlationId}` NDJSON until migrated. Health probes accept both legacy and v1 messaging envelopes where documented.

## Backend proxy

`GET /api/health` and `GET /api/core/status` proxy `homepi-health`. When unreachable, backend returns `degraded` with an explicit user message — never fake `healthy`.
