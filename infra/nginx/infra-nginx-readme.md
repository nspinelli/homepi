# HomePi NGINX Infrastructure

NGINX gateway configuration for HomePi. Not a core service — infrastructure only.

## Purpose

- Serve frontend SPA at `https://homepi.local`
- Proxy `/api/*` to backend
- Proxy WebSocket and SSE routes
- Inject `X-Request-ID` for request correlation

## Paths

| Variable | Default |
|----------|---------|
| `HOMEPI_ROOT` | `/opt/homepi` |
| `HOMEPI_WEB_ROOT` | `/opt/homepi/apps/frontend/dist` |
| `HOMEPI_BACKEND_UPSTREAM` | `127.0.0.1:3000` |

## Install (full operational stack)

From repo root:

```bash
./scripts/install-operational.sh
```

This builds the monorepo, installs NGINX, enables `homepi-backend`, and publishes `homepi.local` via Avahi mDNS.

Verify:

```bash
./scripts/verify-operational.sh
```

LAN devices resolve `http://homepi.local` automatically when mDNS is supported (macOS, iOS, most Linux).

## Install (NGINX only)

```bash
sudo ./infra/nginx/install/install-nginx-config.sh
```
