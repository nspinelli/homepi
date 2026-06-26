# HomePi Backend Application

Node.js + TypeScript API gateway for the HomePi system status vertical slice.

## Purpose

Exposes REST health/status APIs, an SSE system status stream, and a WebSocket shell while wiring all platform concerns through `core/*` modules.

## Development

```bash
pnpm install
pnpm --filter @homepi/app-backend build
pnpm --filter @homepi/app-backend start
```

Listens on `http://127.0.0.1:3000` by default. Configuration is loaded from `config/service-config.json` via `@homepi/core-config`.

## Routes

| Route | Description |
|-------|-------------|
| `GET /api/health` | Health report envelope |
| `GET /api/runtime/status` | Runtime status payload |
| `GET /api/core/status` | Aggregated core platform status |
| `GET /events` | SSE system status + HiFi audio events |
| `GET /ws` | WebSocket upgrade (426 on plain HTTP GET) |
| `GET /api/hifi-serial/health` | HiFi serial native health proxy |
| `POST /api/hifi-serial/sync` | Full controller sync |
| `GET /api/hifi-serial/snapshot` | Cached controller/zones/sources/groups |
| `GET /api/hifi-serial/zones` | Zone list |
| `GET /api/hifi-serial/zones/:n` | Single zone |
| `GET /api/hifi-serial/sources` | Source list |
| `GET /api/hifi-serial/groups` | Group list |
| `POST /api/hifi-serial/commands` | Queue raw `*...` protocol command |
| `GET/PUT /api/audio/paging/config` | Paging configuration (proxied to native service) |
| `GET /api/audio/paging/status` | Paging service status |
| `POST /api/audio/paging/speak` | Publish speak command via broker |
| `POST /api/audio/paging/chime` | Publish chime command via broker |

Additional paging routes under `/api/audio/paging/*` (voices, chimes, preview). See `src/audio/paging/paging-routes.ts`.

Aliases: `/api/events`, `/api/ws`.

## Core Module Usage

- **config** — service config load only entry point for environment-derived settings
- **logging** — structured lifecycle and transport logs (no `console.log`)
- **api** — `createSuccessResponse` / `createErrorResponse` with `correlationId`
- **health** — `/api/health` report generation
- **events** — SSE envelopes via `createEventEnvelope`
- **transport** — WebSocket NDJSON envelopes and ping/pong
- **state** — authoritative in-memory `system.status` snapshot

## Examples

- `examples/core-status-response.example.json`
- `examples/runtime-status-response.example.json`
- `examples/sse-system-status-event.example.json`
- `examples/ws-ping-pong.example.json`

## Tests

```bash
pnpm --filter @homepi/app-backend test
```

Validates response envelope shape, runtime status schema alignment, and SSE envelope examples.

## Native integration

The backend connects to native Unix sockets (`usb-devices`, `hifi-serial`, `pcm-router`, `metadata`, `audio-realtime`, `events`, `audio-paging`) and bridges events to SSE/WebSocket clients. See [docs/architecture/event-flow.md](../../docs/architecture/event-flow.md).
