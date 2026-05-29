# HomePi Metadata

Wrapper service that builds, installs, and supervises [shairport-sync-metadata-reader](https://github.com/mikebrady/shairport-sync-metadata-reader) as part of the HomePi platform.

## Purpose

The upstream tool reads metadata from the FIFO that [Shairport Sync](https://github.com/mikebrady/shairport-sync) writes when `metadata` is enabled in its config. HomePi runs it as a long-lived service with stdin attached to the metadata pipe (default `/tmp/shairport-sync-metadata`).

## Requirements

- Shairport Sync configured to write metadata to the same pipe path
- Pipe readable by the `homepi` user
- Optional: `homepi-nqptp` for AirPlay 2 timing before Shairport starts

## Runtime layout

```text
/opt/homepi/services/metadata/
├── bin/shairport-sync-metadata-reader
├── bin/run-metadata-reader.sh
├── config/service-config.json
└── env/.env                 # HOMEPPI_METADATA_PIPE, HOMEPPI_METADATA_RAW
```

## Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `HOMEPPI_METADATA_PIPE` | `/tmp/shairport-sync-metadata` | Shairport metadata FIFO path |
| `HOMEPPI_METADATA_RAW` | `0` | Set to `1` for `--raw` output |
| `HOMEPPI_METADATA_PIPE_WAIT_SECS` | `120` | Seconds to wait for pipe before creating FIFO |

## Observability

| Interface | Details |
|-----------|---------|
| systemd | `homepi-metadata.service` |
| Logs | `journalctl -u homepi-metadata` (parsed metadata lines) |
| Dashboard | `GET /api/core/status` → `system.metadata` |

## Install

See [services-homepi-metadata-install-readme.md](services-homepi-metadata-install-readme.md).
