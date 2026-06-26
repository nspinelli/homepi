# HomePi Documentation

## Operational

| Document | Description |
|----------|-------------|
| [operational-audit.md](./operational-audit.md) | Install/SSH audit and unit checklist (2026-06) |
| [../scripts/fresh-pi-runbook.md](../scripts/fresh-pi-runbook.md) | Fresh Pi install and configuration |
| [../scripts/scripts-install-readme.md](../scripts/scripts-install-readme.md) | Install scripts, SSH safety, service order |
| [homepi-update.md](./homepi-update.md) | Event-driven audio architecture decisions |
| [homepi-audio-paging-spec.md](./homepi-audio-paging-spec.md) | Audio paging design spec |

## Architecture

| Document | Description |
|----------|-------------|
| [architecture/event-flow.md](./architecture/event-flow.md) | Backend startup and SSE/WebSocket delivery |
| [architecture/service-status.md](./architecture/service-status.md) | Dashboard health mapping rules |
| [architecture/sockets-and-ports.md](./architecture/sockets-and-ports.md) | Unix sockets and network ports |

## Code-adjacent specs

Architecture contracts live alongside code:

- `core/*/core-*-readme.md` — core module specifications
- `apps/*/app-*-readme.md` — application documentation
- `services/*/services-*.md` — native service specs
- `infra/nginx/infra-nginx-readme.md` — gateway configuration
