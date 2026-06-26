# HomePi

Modular, event-driven home automation platform for Raspberry Pi.

## Repository layout

```text
homepi/
├─ core/           # Shared infrastructure (logging, config, transport, events, …)
├─ apps/           # backend, frontend
├─ services/       # native daemons
├─ modules/        # feature modules
├─ infra/nginx/    # gateway configuration
├─ tooling/        # shared dev tools
└─ docs/
```

## Quick start (development)

```bash
pnpm install
pnpm run build
pnpm run test
bash scripts/build-native-services.sh   # subset: USB, HiFi serial, PCM router, metadata
```

## Operational install (Pi)

Installs the web stack, backend, mDNS, Mosquitto, NGINX, SSH hardening, core events broker, and native services (including audio paging):

```bash
sudo bash scripts/install-operational.sh
bash scripts/verify-operational.sh
```

Full fresh-Pi path: [scripts/fresh-pi-runbook.md](scripts/fresh-pi-runbook.md). Audit notes: [docs/operational-audit.md](docs/operational-audit.md).

### Operational units

| Layer | systemd units |
|-------|---------------|
| SSH hardening | `homepi-ensure-ssh` |
| Core | `homepi-events` |
| Native | `homepi-usb-devices`, `homepi-nqptp`, `homepi-pcm-router`, `homepi-metadata`, `homepi-hifi-serial`, `homepi-shairport-supervisor`, `homepi-audio-orchestrator`, `homepi-audio-paging` |
| Web | `nginx`, `homepi-backend` |
| Supporting | `mosquitto`, `avahi-daemon`, `avahi-homepi-alias` |

Preflight (backups, SSH checks) and prerequisites run automatically. See [scripts/scripts-install-readme.md](scripts/scripts-install-readme.md) for SSH/ALSA safety and recovery.

## Development

- Backend: `pnpm --filter @homepi/app-backend start`
- Frontend: `pnpm --filter @homepi/app-frontend dev`
- Gateway: see [infra/nginx/infra-nginx-readme.md](infra/nginx/infra-nginx-readme.md)

Contracts and schemas under `core/` are source of truth. See `.cursorrules` for architectural rules.

## Service status (event-driven)

Dashboard service health is **event-driven**, not poll-driven:

```text
Native daemon → Unix socket / journald / core/events → Backend bridge → SystemStatusStore → SSE / WebSocket → UI
```

- One-time **startup snapshots** on backend boot (`getHealth` / `systemctl`)
- Live updates from native `system.service` events and broker topics
- **Fallback reconciliation** every 120s only for fault recovery (nqptp, metadata, shairport)
- **Uptime** computed at read time from process `startedAt`

See [docs/architecture/event-flow.md](docs/architecture/event-flow.md), [docs/architecture/service-status.md](docs/architecture/service-status.md), and [docs/architecture/sockets-and-ports.md](docs/architecture/sockets-and-ports.md).
