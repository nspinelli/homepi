# homepi-nqptp — Install and validation

## Install (standalone)

```bash
sudo bash /home/homepi/homepi/services/homepi-nqptp/scripts/install.sh
```

## Install (with full HomePi stack)

```bash
sudo bash /home/homepi/homepi/scripts/install-operational.sh
```

Native services are installed via `scripts/install-services.sh` after the TypeScript build.

## Uninstall

```bash
sudo bash /home/homepi/homepi/services/homepi-nqptp/scripts/uninstall.sh
```

## Install layout

```text
/opt/homepi/services/nqptp/
├── bin/nqptp
├── config/service-config.json
└── env/                        # optional .env
```

Upstream version is pinned in `config/service-config.json` (`nqptp.upstreamVersion`, currently **1.2.8**).

## systemd

```bash
sudo systemctl status homepi-nqptp
journalctl -u homepi-nqptp -f
```

## Verify

```bash
systemctl is-active homepi-nqptp
/opt/homepi/services/nqptp/bin/nqptp -V
ss -ulnp | grep -E ':319|:320' || true
curl -sf http://127.0.0.1/api/core/status | jq '.data.system.nqptp'
```

## Firewall

If `ufw` or `firewalld` is enabled, allow UDP **319** and **320** in both directions (PTP).

## Conflicts

- **Other PTP daemons** cannot run alongside nqptp.
- A prior manual install may have left `nqptp.service`; the HomePi install script stops and removes it before enabling `homepi-nqptp.service`.

## Build dependencies

Installed automatically by `install.sh` when missing: `git`, `autoconf`, `automake`, `libtool`, `pkg-config`, `build-essential`.
