# HomePi

Modular, event-driven home automation platform for Raspberry Pi.

## Repository layout

```text
homepi/
├─ core/           # Shared infrastructure (logging, config, transport, …)
├─ apps/           # backend, frontend
├─ services/       # native daemons
├─ modules/        # feature modules
├─ infra/nginx/    # gateway configuration
├─ tooling/        # shared dev tools
└─ docs/
```

## Quick start

```bash
pnpm install
pnpm run build
pnpm run test
```

## Development

- Backend: `pnpm --filter @homepi/app-backend start`
- Frontend: `pnpm --filter @homepi/app-frontend dev`
- Gateway: see [infra/nginx/infra-nginx-readme.md](infra/nginx/infra-nginx-readme.md)

Contracts and schemas under `core/` are source of truth. See `.cursorrules` for architectural rules.
