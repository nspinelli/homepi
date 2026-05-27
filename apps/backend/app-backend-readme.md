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
| `GET /events` | SSE system status stream |
| `GET /ws` | WebSocket upgrade (426 on plain HTTP GET) |

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

## Not Included Yet

Module command handling, persistent state, authentication enforcement, and native service integration.
