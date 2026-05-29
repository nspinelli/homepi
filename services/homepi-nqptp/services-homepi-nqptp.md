# HomePi NQPTP

Wrapper service that builds, installs, and supervises [nqptp](https://github.com/mikebrady/nqptp) (Not Quite PTP) as part of the HomePi platform.

## Purpose

`nqptp` monitors timing data from PTP clocks on UDP ports **319** and **320**. It is the timing companion for [Shairport Sync](https://github.com/mikebrady/shairport-sync) during AirPlay 2 operation.

HomePi does not reimplement nqptp. This folder provides:

- reproducible build and install under `/opt/homepi/services/nqptp`
- a HomePi systemd unit (`homepi-nqptp.service`)
- `service-manifest.json` and `service-config.json` contracts
- integration with backend core status (systemd health) and journal log streaming

## Requirements

- Exclusive use of UDP ports 319 and 320 (no other PTP daemons)
- `CAP_NET_BIND_SERVICE` via systemd ambient capabilities
- Network available at startup (`network-online.target`)

## Runtime layout

```text
/opt/homepi/services/nqptp/
├── bin/nqptp
├── config/service-config.json
└── env/.env                    # optional overrides
```

## Control and observability

| Interface | Details |
|-----------|---------|
| systemd | `homepi-nqptp.service` |
| Control port | 9000 (upstream nqptp protocol) |
| Shared memory | POSIX SHM (see upstream README) |
| Logs | `journalctl -u homepi-nqptp` |
| Dashboard | `GET /api/core/status` → `system.nqptp` |

## Related services

- **Shairport Sync** (planned): must start after nqptp; unit uses `Before=shairport-sync.service`
- **homepi-metadata**: reads Shairport metadata FIFO; often deployed alongside nqptp
- **homepi-usb-devices / homepi-hifi-serial**: independent; no runtime dependency

## Install

See [services-homepi-nqptp-install-readme.md](services-homepi-nqptp-install-readme.md).

Or install all native services via the root operational installer:

```bash
sudo bash /home/homepi/homepi/scripts/install-operational.sh
```
