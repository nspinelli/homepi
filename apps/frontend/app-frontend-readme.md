# HomePi Frontend Application

React + Vite + TypeScript shell for the HomePi web app, based on the `homepi-app-design` reference.

## Purpose

Provides a routed HomePi UI with an empty home page, a live system status page, and a settings placeholder. Validates REST, SSE, and WebSocket connectivity to the backend vertical slice.

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

## User Settings

Appearance preferences are stored in `localStorage` under `homepi:user-settings`:

```json
{
  "appearance": {
    "theme": "light" | "dark" | "system"
  }
}
```

## Routes

| Path | Description |
|------|-------------|
| `/` | Empty home page (reserved for future modules) |
| `/status` | System status dashboard |
| `/settings` | User preferences (appearance theme) |

## Status Page Behavior

Displays:

- Backend health from `GET /api/health`
- Core service statuses from `GET /api/core/status` and live SSE deltas
- Uptime and last event timestamp
- SSE and WebSocket connection state
- Rolling live event log from SSE envelopes

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

Module UIs on the home page, authentication flows, and production asset hardening beyond the Vite build.
