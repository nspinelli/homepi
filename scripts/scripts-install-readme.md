# HomePi install scripts

Operational install scripts for the Raspberry Pi production stack.

## Quick install

See **[fresh-pi-runbook.md](./fresh-pi-runbook.md)** for the full path from bare Pi to configured production state (install + UI assignments + AirPlay).

```bash
cd /home/homepi/homepi
sudo bash scripts/install-preflight.sh    # optional if using install-operational (runs automatically)
sudo bash scripts/install-operational.sh
bash scripts/verify-operational.sh
```

`install-operational.sh` runs preflight, prerequisites, `pnpm install`, build, all six native services, NGINX, backend, and Avahi.

## Script reference

| Script | Purpose |
|--------|---------|
| `install-preflight.sh` | Backs up SSH/sudoers/modprobe configs; checks sshd enabled |
| `install-prerequisites.sh` | Single apt transaction for all build + runtime deps |
| `install-operational.sh` | Full stack install |
| `install-services.sh` | Native services only (dependency order) |
| `verify-operational.sh` | Service, MQTT, ALSA, HTTP health checks |
| `verify-post-reboot.sh` | Run after a controlled reboot test |
| `fresh-pi-runbook.md` | End-to-end fresh Pi install and configuration guide |
| `verify-nqptp-patch.sh` | Offline check that nqptp patch applies to pinned upstream |
| `lib/install-common.sh` | Shared backup, apt, sudoers, patch helpers |

## Prerequisites

- Raspberry Pi OS with user `homepi`
- Node.js >= 20 and pnpm >= 9 (installed via `nodejs` apt package + corepack when missing)
- User `homepi` in group `dialout` for HiFi serial (`install-prerequisites.sh --ensure-dialout`)
- Active SSH session recommended; keep serial console as fallback

## SSH safety

Install scripts **do not modify** `sshd_config` or firewall rules.

Preflight backs up to `/var/backups/homepi/install-<timestamp>/`:

- `/etc/ssh/sshd_config`
- `/etc/sudoers` and `/etc/sudoers.d/*`
- `/etc/modprobe.d/homepi-*.conf`, `/etc/modules-load.d/homepi-*.conf`
- `/proc/asound/cards` snapshot

Sudoers drop-ins are validated with `visudo -cf` per file and `visudo -c` for the full config.

Hook scripts referenced by sudoers are installed as `root:root` mode `0755` so the `homepi` user cannot replace them.

### Recovery

If SSH fails after reboot, boot with serial console and restore from the latest backup:

```bash
sudo cp -a /var/backups/homepi/install-<timestamp>/etc/ssh/sshd_config /etc/ssh/sshd_config
sudo cp -a /var/backups/homepi/install-<timestamp>/etc/modprobe.d/* /etc/modprobe.d/
sudo systemctl restart ssh
```

## ALSA and reboots

- **Install** sets `HOMEPI_INSTALL_MODE=1` and `HOMEPI_ALLOW_REBOOT=0` — no automatic reboot during install.
- **pcm-router** installs `snd-aloop` loopback config; loopback cards may require a manual reboot.
- **USB primary audio** deploy is skipped during install; it runs only when saving assignments in the UI.
- **post-assignment-hook** will not auto-reboot by default (`HOMEPI_ALLOW_REBOOT=0`). Set `HOMEPI_ALLOW_REBOOT=1` in `/opt/homepi/services/usb-devices/env/.env` to re-enable after primary audio rebind fails.

Test UI assignment saves safely:

```bash
HOMEPI_ALLOW_REBOOT=0 sudo -u homepi sudo /opt/homepi/services/usb-devices/scripts/post-assignment-hook.sh
```

Optional delay before reboot: `HOMEPI_REBOOT_DELAY_SEC=10`

## nqptp firewall

If `ufw` or `firewalld` is enabled, allow UDP **319** and **320** (PTP) for AirPlay 2 timing.

## Native services (install order)

1. `homepi-usb-devices`
2. `homepi-nqptp` (patched upstream 1.2.8)
3. `homepi-pcm-router`
4. `homepi-metadata`
5. `homepi-hifi-serial`
6. `homepi-shairport-sync`

## Post-reboot verification

After a controlled `sudo reboot`:

```bash
bash scripts/verify-post-reboot.sh
```

Compare ALSA layout with the preflight snapshot under `/var/backups/homepi/`.
