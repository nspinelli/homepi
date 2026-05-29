# homepi-metadata scripts

| Script | Purpose |
|--------|---------|
| `install.sh` | Clone upstream, build to `/opt/homepi/services/metadata`, install systemd unit |
| `uninstall.sh` | Stop service, remove unit and install tree |
| `run-metadata-reader.sh` | Installed wrapper: waits for FIFO, runs metadata reader on stdin |
