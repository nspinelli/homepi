# HomePi Metadata

Single native C++20 daemon (`homepi-metadata`) that implements the pipe-only metadata design from `docs/homepi-update.md`.

## Purpose

Shairport Sync writes metadata to per-zone FIFOs (`/tmp/homepi-metadata-zone-N`) with MQTT disabled in generated zone configs. The metadata service:

- Subscribes to `core/events` for PCM owner changes (`owner_changed`, enabled zones)
- Opens enabled zone pipes in one `epoll` loop (non-blocking)
- **Parses only the PCM owner pipe** into the global now-playing reducer
- **Drains and discards** bytes from enabled non-owner pipes (pipes never block Shairport)
- Coalesces track/client field updates (~250 ms) before emitting to `core/events`
- Publishes playback progress through `/run/homepi/audio-realtime.sock` (not the event bus)
- Persists current now-playing and last-20 play history via `core/storage` (SQLite)
- Caches cover art on disk (content-addressed + `current.jpg`)

There is **no Python** and **no MQTT** on the metadata runtime path.

## Parser coverage

Pipe items are parsed in C++ (`metadata-parser.cpp`) for `core/*` and `ssnc/*` codes defined in `docs/homepi-update.md` §11.1, including:

- Track fields: `minm`, `asar`, `asal`, `astm`, `mper`
- Client fields: `snam`, `cmod`
- Progress: `prgr`, `phb0`, `phbt`
- Cover art: `PICT`
- Bundle boundaries: `mdst`, `mden`
- Session lifecycle: `pbeg`, `pend`, `paus`, `prsm`

## Core modules

| Module | Usage |
|--------|--------|
| `core/logging` | Structured service lifecycle logs |
| `core/events` | NDJSON envelopes to backend SSE bridge |
| `core/storage` | SQLite `audio_now_playing` + `audio_play_history` |
| `core/transport` | Latest-value realtime socket publisher |

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
| `HOMEPI_EVENTS_SOCKET` | `/run/homepi/broker/broker.sock` | v2 event broker publish/subscribe |
| `HOMEPI_PCM_ROUTER_SOCKET` | `/run/homepi/audio/pcm-router.sock` | PCM router bootstrap subscription |
| `HOMEPI_AUDIO_REALTIME_SOCKET` | `/run/homepi/audio/audio-realtime.sock` | Progress realtime endpoint |
| `HOMEPI_DATABASE_PATH` | `/opt/homepi/runtime/state/homepi.sqlite` | Shared SQLite database |
| `HOMEPI_CACHE_DIR` | `/opt/homepi/runtime/cache` | Cover art cache directory |
| `HOMEPI_METADATA_PIPE_PREFIX` | `/tmp/homepi-metadata-zone-` | Shairport FIFO prefix |
| `HOMEPI_ZONE_COUNT` | `16` | Number of zone pipes to monitor |
| `METADATA_DEBOUNCE_MS` | `250` | Coalesce timer for track/client emits |
| `LOG_LEVEL` | `INFO` | Log verbosity |

## Events

| topic | event | When |
|-------|-------|------|
| `modules.metadata.snapshot` | `metadata_snapshot` | Subscribe, owner change, coalesced track update |
| `modules.metadata.now_playing` | `metadata_track_changed` | After bundle/coalesce flush |
| `modules.metadata.cover_art` | `metadata_cover_updated` | Cover art saved |
| `modules.metadata.progress` | `playback_state_changed` | Pause/resume/stop (not periodic ticks) |
| `modules.metadata.now_playing` | `metadata_cleared` | Owner cleared or session end |

Progress position/duration streams on `audio-realtime.sock` as `audio.realtime.snapshot`.

## systemd

`homepi-metadata.service` — single unit (legacy `homepi-metadata@N` template is disabled on install).

## Install

```bash
sudo bash services/homepi-metadata/scripts/install.sh
```
