# HomePi Frontend Application

React + Vite + TypeScript shell for the HomePi system status dashboard.

## Purpose

Provides a minimal Apple-inspired dashboard that validates REST, SSE, and WebSocket connectivity to the backend vertical slice.

## Development

```bash
pnpm install
pnpm --filter @homepi/app-frontend dev
```

Open `http://127.0.0.1:5173`. Vite proxies `/api`, `/events`, and `/ws` to the backend on port `3000`.

## Configuration

| Variable | Description |
|----------|-------------|
| `VITE_API_BASE_URL` | REST API base (default: same origin) |
| `VITE_EVENTS_URL` | SSE endpoint (default: `{base}/events`) |
| `VITE_WS_URL` | WebSocket endpoint (default: `{ws-base}/ws`) |

## Dashboard Behavior

Displays:

- Backend health from `GET /api/health`
- Core service statuses from `GET /api/core/status` and live SSE deltas
- Uptime and last event timestamp
- SSE and WebSocket connection state
- Last received event envelope (JSON)

## Tests

```bash
pnpm --filter @homepi/app-frontend test
```

Runs Vitest smoke checks. Build/typecheck:

```bash
pnpm --filter @homepi/app-frontend build
pnpm --filter @homepi/app-frontend typecheck
```

## Not Included Yet

Module UIs, authentication flows, routing beyond the system dashboard, and production asset hardening beyond the Vite build.
