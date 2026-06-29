# HomePi install scripts

Operational install scripts for the Raspberry Pi production stack.

## Quick install

See **[fresh-pi-runbook.md](./fresh-pi-runbook.md)** for the full path from bare Pi to configured production state (install + UI assignments + AirPlay + paging).

```bash
cd /home/homepi/homepi
sudo bash scripts/install-operational.sh
bash scripts/verify-operational.sh
```

`install-operational.sh` runs preflight, SSH hardening (`homepi-ensure-ssh`), prerequisites, `pnpm install`, build, Node platform services (`homepi-broker`, `homepi-health`, facades), eight native services (including paging), NGINX, backend, and Avahi.

## Script reference

| Script | Purpose |
|--------|---------|
| `install-preflight.sh` | Backs up SSH/sudoers/modprobe configs; checks sshd enabled |
| `install-prerequisites.sh` | Single apt transaction for all build + runtime deps |
| `install-operational.sh` | Full stack install |
| `install-services.sh` | Native services only (dependency order) |
| `install-ensure-ssh.sh` | SSH boot hardening only (`homepi-ensure-ssh`) |
| `ensure-ssh-access.sh` | Masks `ssh.socket`, enables `ssh.service`, repairs key perms, backs up keys |
| `uninstall-ensure-ssh.sh` | Removes `homepi-ensure-ssh` unit and script |
| `verify-operational.sh` | Service, MQTT, ALSA, HTTP health checks |
| `verify-v2-signoff.sh` | Phase E: v2 negative checks, API shape, broker/health isolation |
| `verify-post-reboot.sh` | Run after a controlled reboot test (includes SSH check) |
| `fresh-pi-runbook.md` | End-to-end fresh Pi install and configuration guide |
| `verify-nqptp-patch.sh` | Offline check that nqptp patch applies to pinned upstream |
| `lib/install-common.sh` | Shared backup, apt, sudoers, patch helpers |

## Prerequisites

- Raspberry Pi OS (64-bit) with user `homepi`
- Node.js >= 20 and pnpm >= 9 (installed via `nodejs` apt package + corepack when missing)
- User `homepi` in group `dialout` for HiFi serial (`install-prerequisites.sh --ensure-dialout`)
- Active SSH session recommended; keep serial console as fallback

## Operational stack components

### Installed by `install-operational.sh`

| Layer | Units / artifacts |
|-------|-------------------|
| SSH hardening | `homepi-ensure-ssh` |
| Platform | `homepi-broker`, `homepi-health`, `homepi-audio`, `homepi-sensors` |
| Native services | `homepi-usb-devices`, `homepi-nqptp`, `homepi-pcm-router`, `homepi-metadata`, `homepi-hifi-serial`, `homepi-shairport-supervisor`, `homepi-audio-orchestrator`, `homepi-audio-paging` |
| Web | `nginx`, `homepi-backend` |
| Messaging | `mosquitto` |
| mDNS | `avahi-daemon`, `avahi-homepi-alias` |

### Standalone install

| Component | Command |
|-----------|---------|
| SSH boot hardening only | `sudo bash scripts/install-ensure-ssh.sh` |
| Audio paging only | `sudo bash services/homepi-audio-paging/scripts/install.sh` |

## SSH safety

Install scripts **do not modify** `sshd_config` or firewall rules.

### Preflight (`install-preflight.sh`)

Backs up to `/var/backups/homepi/install-<timestamp>/`:

- `/etc/ssh/sshd_config`
- `/etc/sudoers` and `/etc/sudoers.d/*`
- `/etc/modprobe.d/homepi-*.conf`, `/etc/modules-load.d/homepi-*.conf`
- `/proc/asound/cards` snapshot

Verifies `ssh.service` (or `sshd.service`) is **enabled**. Does not mask `ssh.socket`.

Sudoers drop-ins are validated with `visudo -cf` per file and `visudo -c` for the full config.

Hook scripts referenced by sudoers are installed as `root:root` mode `0755` so the `homepi` user cannot replace them.

### Boot hardening (`homepi-ensure-ssh`)

Unit file: `infra/systemd/homepi-ensure-ssh.service`  
Script: `scripts/ensure-ssh-access.sh` (installed to `/opt/homepi/scripts/`)

On each boot (after `network-online.target`):

1. Masks `ssh.socket` (avoids socket-activation conflicts on Pi OS)
2. Enables and starts `ssh.service`
3. Repairs `~/.ssh` and `authorized_keys` permissions
4. Backs up keys and `sshd_config` to `/var/backups/homepi/ssh/`

**SSH starts before this oneshot runs** (~9 s vs ~13 s on typical boot). The oneshot does not block initial SSH availability; it prevents post-reboot SSH loss from socket/perms issues.

Install once (also run automatically by `install-operational.sh`):

```bash
sudo bash scripts/install-ensure-ssh.sh
```

Optional preflight repair during install:

```bash
sudo bash scripts/install-preflight.sh --repair-ssh-perms
```

### Recovery

If SSH fails after reboot, boot with serial console and restore from the latest backup:

```bash
sudo cp -a /var/backups/homepi/install-<timestamp>/etc/ssh/sshd_config /etc/ssh/sshd_config
sudo cp -a /var/backups/homepi/install-<timestamp>/etc/modprobe.d/* /etc/modprobe.d/
sudo cp -a /var/backups/homepi/ssh/authorized_keys.latest /home/homepi/.ssh/authorized_keys
sudo chmod 700 /home/homepi/.ssh
sudo chmod 600 /home/homepi/.ssh/authorized_keys
sudo chown -R homepi:homepi /home/homepi/.ssh
sudo systemctl mask ssh.socket
sudo systemctl enable ssh
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

`install-services.sh` order:

1. `homepi-usb-devices`
2. `homepi-nqptp` (patched upstream 1.2.8)
3. `homepi-pcm-router`
4. `homepi-metadata`
5. `homepi-hifi-serial`
6. `homepi-shairport-sync`
7. `homepi-audio-orchestrator`
8. `homepi-audio-paging`

`install-operational.sh` also installs `homepi-ensure-ssh` (after preflight) and Node platform services (`install-node-services.sh`) before native services.

## Post-reboot verification

After a controlled `sudo reboot`:

```bash
bash scripts/verify-post-reboot.sh
```

Compare ALSA layout with the preflight snapshot under `/var/backups/homepi/`.

## Uninstall

`uninstall-operational.sh` removes backend, Avahi alias, native services (reverse install order), and `homepi-ensure-ssh`. NGINX site is left in place.

`uninstall-services.sh` reverse order:

1. `homepi-audio-paging`
2. `homepi-audio-orchestrator`
3. `homepi-shairport-sync`
4. `homepi-hifi-serial`
5. `homepi-metadata`
6. `homepi-pcm-router`
7. `homepi-nqptp`
8. `homepi-usb-devices`

`build-native-services.sh` builds four services for local dev; production install uses per-service `install.sh` scripts.

See [operational-audit.md](../docs/operational-audit.md) for the install audit.
