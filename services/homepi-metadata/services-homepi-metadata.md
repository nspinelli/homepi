# HomePi Metadata

Single C++ daemon that drains all Shairport metadata FIFOs (so pipes never block) and consumes parsed MQTT metadata for the PCM router owner zone.

## Purpose

Shairport Sync writes metadata to per-zone pipes (`/tmp/homepi-metadata-zone-N`) and publishes parsed fields to MQTT (`shairport/zone/N/...`). The metadata service:

- Subscribes to `homepi-pcm-router` for `ownerZoneId`
- Drains every zone pipe via one `epoll` loop (non-blocking, no FIFO parsing)
- Subscribes to Shairport MQTT topics for the active PCM owner zone
- Emits `core/events` envelopes on `/run/homepi/metadata.sock`
- Persists now-playing state via `core/storage` (SQLite)

## MQTT topics (owner zone)

| Topic suffix | Maps to |
|--------------|---------|
| `title`, `artist`, `album`, `client_name` | Now-playing text fields |
| `playing` | Playback state (`1` / `0`) |
| `cover` | Album art (binary) |
| `frame_position_and_time` | Progress position (with `first_frame_position_and_time`) |
| `ssnc/prgr`, `core/astm` | Progress position + duration (requires `publish_raw = yes` on Shairport) |
| `active_start` | Clear track metadata for new session |
| `active_end`, `play_end` | Session cleared |

## Core modules

| Module | Usage |
|--------|--------|
| `core/logging` | Structured service lifecycle logs |
| `core/events` | NDJSON event envelopes to backend SSE bridge |
| `core/storage` | SQLite persistence for owner-zone now-playing |

## Runtime layout

```text
/opt/homepi/services/metadata/
├── bin/homepi-metadata
├── config/service-config.json
└── env/.env
```

## Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `HOMEPI_EVENT_SOCKET` | `/run/homepi/metadata.sock` | Metadata Unix API socket |
| `HOMEPI_PCM_ROUTER_SOCKET` | `/run/homepi/pcm-router.sock` | PCM router subscription |
| `HOMEPI_DATABASE_PATH` | `/opt/homepi/runtime/state/metadata.db` | SQLite state database |
| `HOMEPI_CACHE_DIR` | `/opt/homepi/runtime/cache` | Cover art cache directory |
| `HOMEPI_METADATA_PIPE_PREFIX` | `/tmp/homepi-metadata-zone-` | Shairport FIFO prefix |
| `HOMEPI_ZONE_COUNT` | `16` | Number of zone pipes to monitor |
| `MQTT_HOST` | `127.0.0.1` | Mosquitto broker hostname |
| `MQTT_PORT` | `1883` | Mosquitto broker port |
| `MQTT_TOPIC_PREFIX` | `shairport/zone` | Shairport MQTT topic prefix before zone id |
| `LOG_LEVEL` | `INFO` | Log verbosity |

## Events

| topic | event | When |
|-------|-------|------|
| `modules.metadata.snapshot` | `metadata_snapshot` | Subscribe + owner change |
| `modules.metadata.now_playing` | `metadata_field_updated` | Owner zone title/artist/album/client |
| `modules.metadata.progress` | `metadata_progress_updated` | Owner zone progress |
| `modules.metadata.cover_art` | `metadata_cover_updated` | Owner zone cover art |
| `modules.metadata.now_playing` | `metadata_cleared` | Owner cleared or session end |

## systemd

`homepi-metadata.service` — single unit (replaces legacy `homepi-metadata@N` template).

## Install

```bash
sudo bash services/homepi-metadata/scripts/install.sh
```
