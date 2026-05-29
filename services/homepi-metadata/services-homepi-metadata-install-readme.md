# homepi-metadata — Install and validation

## Install (standalone)

```bash
sudo bash /home/homepi/homepi/services/homepi-metadata/scripts/install.sh
```

## Install (with full HomePi stack)

```bash
sudo bash /home/homepi/homepi/scripts/install-operational.sh
```

## Uninstall

```bash
sudo bash /home/homepi/homepi/services/homepi-metadata/scripts/uninstall.sh
```

## Shairport Sync pipe

In `shairport-sync.conf` (when Shairport is installed), enable metadata output to the same path:

```conf
metadata =
{
    enabled = "yes";
    include_cover_art = "yes";
    pipe_name = "/tmp/shairport-sync-metadata";
};
```

Override the path via `/opt/homepi/services/metadata/env/.env`:

```bash
HOMEPPI_METADATA_PIPE=/tmp/shairport-sync-metadata
```

## Verify

```bash
systemctl is-active homepi-metadata
journalctl -u homepi-metadata -n 20 --no-pager
curl -sf http://127.0.0.1/api/core/status | jq '.data.system.metadata'
```

## Build dependencies

Installed automatically when missing: `git`, `autoconf`, `automake`, `libtool`, `pkg-config`, `build-essential`.
