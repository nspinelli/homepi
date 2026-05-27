# HomePi Backend Application

Node.js + TypeScript API gateway shell for HomePi.

## Development

Assumes `homepi.local` via NGINX in production. Local dev:

```bash
pnpm --filter @homepi/app-backend build
pnpm --filter @homepi/app-backend start
```

## Routes (shell)

| Route | Purpose |
|-------|---------|
| `GET /api/health` | Health report |
| `GET /api/events` | SSE placeholder |
| `GET /api/ws` | WebSocket upgrade placeholder |

Uses `@homepi/core-logging`, `@homepi/core-config`, and `@homepi/core-api`.
