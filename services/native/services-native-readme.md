# HomePi Native Services

C++ daemons and native services under `services/`. Use `core/logging` (C++) and `core/transport` (C++) headers. Control-plane messaging uses `core/events` (`/run/homepi/events.sock`) where noted.

## Production install order

Installed by `scripts/install-services.sh` (via `install-operational.sh`):

| Service | systemd unit | Socket / notes |
|---------|--------------|----------------|
| USB devices | `homepi-usb-devices` | `/run/homepi/usb-devices.sock` |
| NQPTP | `homepi-nqptp` | journald only |
| PCM router | `homepi-pcm-router` | `/run/homepi/pcm-router.sock` |
| Metadata | `homepi-metadata` | `/run/homepi/metadata.sock`, `/run/homepi/audio-realtime.sock` |
| HiFi serial | `homepi-hifi-serial` | `/run/homepi/hifi-serial.sock` |
| Shairport sync | `homepi-shairport-supervisor`, `homepi-shairport@N` | Mosquitto remote |
| Audio orchestrator | `homepi-audio-orchestrator` | broker only |
| Audio paging | `homepi-audio-paging` | `/run/homepi/audio-paging.sock` |

**Core broker (not under `services/`):** `homepi-events` — `core/events/scripts/install.sh`

**SSH hardening:** `homepi-ensure-ssh` — `scripts/install-ensure-ssh.sh` (via `install-operational.sh`)

## Per-service docs

| Directory | Doc |
|-----------|-----|
| `homepi-usb-devices/` | [services-homepi-usb-devices.md](./homepi-usb-devices/services-homepi-usb-devices.md) |
| `homepi-nqptp/` | [services-homepi-nqptp.md](./homepi-nqptp/services-homepi-nqptp.md) |
| `homepi-pcm-router/` | [services-homepi-pcm-router.md](./homepi-pcm-router/services-homepi-pcm-router.md) |
| `homepi-metadata/` | [services-homepi-metadata.md](./homepi-metadata/services-homepi-metadata.md) |
| `homepi-hifi-serial/` | [services-homepi-hifi-serial.md](./homepi-hifi-serial/services-homepi-hifi-serial.md) |
| `homepi-shairport-sync/` | [services-homepi-shairport-sync.md](./homepi-shairport-sync/services-homepi-shairport-sync.md) |
| `homepi-audio-orchestrator/` | (see `docs/homepi-update.md`) |
| `homepi-audio-paging/` | [services-homepi-audio-paging.md](./homepi-audio-paging/services-homepi-audio-paging.md) |

## Dev build subset

`scripts/build-native-services.sh` builds USB, HiFi serial, PCM router, and metadata only. Full Pi install uses each service's `scripts/install.sh`.
