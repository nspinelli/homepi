# Fresh Pi Runbook

End-to-end guide to bring a **new Raspberry Pi** from bare OS to the same operational state as a configured HomePi production unit: services running, USB roles assigned, stable primary ALSA name, HiFi zones synced, AirPlay ready, and optional audio paging.

See also [operational-audit.md](../docs/operational-audit.md) for the full audit of install gaps and SSH boot behavior.

## Prerequisites

- Raspberry Pi OS (64-bit) with user **`homepi`**
- Network access (LAN; mDNS for `http://homepi.local`)
- SSH or serial console (keep serial as fallback during first install)
- HomePi hardware connected **before** configuration:
  - Primary serial adapter (FT232 / HiFi controller)
  - Primary USB audio DAC (Apple USB-C or assigned DAC)
  - Paging USB audio output(s) (if using audio paging)
- Git clone of [homepi](https://github.com/nspinelli/homepi) on the Pi, e.g. `/home/homepi/homepi`

## Phase 0 — OS imaging and SSH baseline

1. Flash **Raspberry Pi OS (64-bit)** with Raspberry Pi Imager.
2. In Imager advanced options (or first boot):
   - Create user **`homepi`**
   - Enable SSH (password or public-key authentication)
   - Set hostname if desired (e.g. `homepi`)
3. Boot the Pi and confirm SSH from your workstation:

```bash
ssh homepi@homepi.local
# or: ssh homepi@<pi-ip>
```

4. Install your SSH public key if not already present:

```bash
mkdir -p ~/.ssh && chmod 700 ~/.ssh
# from your workstation:
# ssh-copy-id homepi@homepi.local
```

5. Clone the repository:

```bash
cd /home/homepi
git clone https://github.com/nspinelli/homepi.git
cd homepi
```

6. **SSH hardening** runs automatically during `install-operational.sh` (`homepi-ensure-ssh`). To install it alone:

```bash
sudo bash scripts/install-ensure-ssh.sh
```

## Phase 1 — One-command install

From the repo root:

```bash
cd /home/homepi/homepi
sudo bash scripts/install-operational.sh
```

This runs preflight backups, SSH hardening, apt prerequisites, `pnpm install`, build, Node platform services (`homepi-broker`, `homepi-health`, facades), eight native services (see below), NGINX, backend, Avahi, and `verify-operational.sh`.

**Native services installed (in order):**

1. `homepi-usb-devices`
2. `homepi-nqptp`
3. `homepi-pcm-router`
4. `homepi-metadata`
5. `homepi-hifi-serial`
6. `homepi-shairport-sync`
7. `homepi-audio-orchestrator`
8. `homepi-audio-paging`

**Also installed:** `homepi-ensure-ssh`, `homepi-broker`, `homepi-health`, `homepi-audio`, `homepi-sensors`, `homepi-backend`, `nginx`, `mosquitto`, `avahi-daemon`, `avahi-homepi-alias`.

**Expected:** install completes with most services `active`. Warnings about missing `HomePiPrimary` or loopback cards before first USB save are normal.

**Install intentionally does not:**

- Assign USB roles (serial / primary audio / paging)
- Deploy primary-audio modprobe (`HOMEPI_INSTALL_MODE=1`)
- Auto-reboot (`HOMEPI_ALLOW_REBOOT=0`)

If verification fails, inspect:

```bash
journalctl -u homepi-usb-devices -n 50 --no-pager
bash scripts/verify-operational.sh
```

Backups from preflight: `/var/backups/homepi/install-<timestamp>/`

## Phase 2 — Confirm platform health

```bash
bash scripts/verify-operational.sh
curl -sf http://127.0.0.1/api/usb-devices/health | head -c 200
echo
ls -l /run/homepi/*.sock
```

Open in a browser (same LAN): **http://homepi.local**

Status page should show USB Devices, backend, and core services healthy.

## Phase 3 — USB and audio configuration (UI)

Go to **Settings → Audio Configuration**.

1. Confirm all connected USB devices appear in the dropdowns (serial + audio).
2. Assign:
   - **Primary Serial Connection** — FT232 / HiFi adapter
   - **Primary Audio Output** — main DAC
   - **Primary Audio Profile** — e.g. `44100 Hz · S16_LE · Stereo`
   - **Primary Paging Output** — paging DAC (if using paging)
3. Click **Save**.

Saving triggers `post-assignment-hook.sh` (as root via sudoers), which:

- Deploys udev rules → `/dev/vHifi`
- Restarts `homepi-hifi-serial`
- Deploys modprobe for stable **`HomePiPrimary`** ALSA card name
- Runs full USB-audio reload (no full Pi reboot when successful)
- Restarts `homepi-pcm-router` and `homepi-shairport-supervisor`

Hook log:

```bash
tail -50 /opt/homepi/runtime/cache/post-assignment-hook.log
```

### Verify primary audio without reboot

```bash
grep HomePiPrimary /proc/asound/cards
systemctl is-active homepi-pcm-router homepi-hifi-serial
```

If `HomePiPrimary` is missing after save:

```bash
sudo bash /opt/homepi/services/usb-devices/scripts/apply-primary-audio-alsa.sh
sudo systemctl restart homepi-pcm-router homepi-shairport-supervisor
```

Optional opt-in auto-reboot when live apply fails: set `HOMEPI_ALLOW_REBOOT=1` in `/opt/homepi/services/usb-devices/env/.env` and restart `homepi-usb-devices`.

## Phase 4 — HiFi controller sync

After serial assignment and `/dev/vHifi`:

```bash
ls -la /dev/vHifi
timeout 3 bash -c 'printf "%s\n" "{\"method\":\"getHealth\",\"correlationId\":\"runbook\"}" | nc -U /run/homepi/audio/hifi-serial.sock | head -1'
sqlite3 /opt/homepi/runtime/state/homepi.sqlite "SELECT COUNT(*) FROM hifi_zones;"
```

**Expected:** health JSON with controller connected; zone count matches your HiFi hardware (e.g. 16).

If sync fails:

```bash
sudo systemctl restart homepi-hifi-serial
journalctl -u homepi-hifi-serial -n 40 --no-pager
```

## Phase 5 — AirPlay source and zones

In the UI **Sources** section, set the AirPlay input source (e.g. source **5** on this deployment).

Verify Shairport:

```bash
systemctl is-active homepi-shairport-supervisor homepi-nqptp homepi-audio-orchestrator
systemctl list-units 'homepi-shairport@*' --no-pager | head -15
sqlite3 /opt/homepi/runtime/state/homepi.sqlite "SELECT id, name, is_airplay FROM hifi_sources WHERE is_airplay=1;"
```

**Expected:** supervisor active; one `hifi_sources` row with `is_airplay=1`; zone units running for configured zones.

## Phase 6 — Final verification checklist

| Check | Command / location |
|-------|-------------------|
| SSH hardened | `systemctl is-enabled homepi-ensure-ssh`; `systemctl is-enabled ssh.service`; `systemctl is-enabled ssh.socket` → masked |
| All USB devices listed | Settings dropdowns or `curl -s http://127.0.0.1/api/usb-devices` |
| Assignments saved | `curl -s http://127.0.0.1/api/usb-devices/assignments` |
| Primary profile visible | Settings → Primary Audio Profile populated |
| `HomePiPrimary` ALSA | `grep HomePiPrimary /proc/asound/cards` |
| pcm-router healthy | `systemctl is-active homepi-pcm-router` |
| HiFi zones | `sqlite3 ... "SELECT COUNT(*) FROM hifi_zones;"` |
| AirPlay source | UI Sources + `hifi_sources.is_airplay=1` |
| Events broker | `test -S /run/homepi/broker/broker.sock` |
| Paging (if installed) | `systemctl is-active homepi-audio-paging` |
| Operational stack | `bash scripts/verify-operational.sh` |

After an optional controlled reboot:

```bash
sudo reboot
# when back:
bash scripts/verify-post-reboot.sh
bash scripts/verify-operational.sh
```

## Troubleshooting

### Only one USB device in Settings

USB-audio reload may have left devices unbound. Check:

```bash
for dev in /sys/bus/usb/devices/*-*; do
  [[ -f "$dev/idVendor" ]] || continue
  echo "$(basename "$dev") driver=$(readlink "$dev/driver" 2>/dev/null | xargs basename || echo none)"
done
```

Any `driver=none` on hardware ports → rebind:

```bash
sudo modprobe snd-usb-audio usbserial ftdi_sio
for p in 1-1 1-2 3-1 3-2; do echo "$p" | sudo tee /sys/bus/usb/drivers/usb/bind 2>/dev/null; done
sudo systemctl restart homepi-usb-devices
```

Then re-save assignments in the UI.

### Primary Audio Profile dropdown empty

Usually stale assignment ALSA index or DAC held open by pcm-router. Re-save assignments; the service heals IDs by USB identity and falls back to stored profile capabilities. Confirm:

```bash
curl -s "http://127.0.0.1/api/usb-devices/assignments"
curl -s "http://127.0.0.1/api/usb-devices/<primary-device-id>/audio-capabilities"
```

### SSH lost after reboot

1. Use serial console or physical access.
2. Restore from preflight backup (see `scripts-install-readme.md` → SSH recovery).
3. Or restore keys from ensure-ssh backups:

```bash
sudo cp -a /var/backups/homepi/ssh/authorized_keys.latest /home/homepi/.ssh/authorized_keys
sudo chmod 700 /home/homepi/.ssh
sudo chmod 600 /home/homepi/.ssh/authorized_keys
sudo chown -R homepi:homepi /home/homepi/.ssh
sudo systemctl mask ssh.socket
sudo systemctl enable ssh
sudo systemctl restart ssh
```

4. Re-run SSH hardening (Phase 0 step 6) once access is restored.

## Related docs

- [scripts-install-readme.md](./scripts-install-readme.md) — script reference, SSH/ALSA safety
- [operational-audit.md](../docs/operational-audit.md) — audit findings and unit checklist
- [services/homepi-usb-devices/services-homepi-usb-devices-install-readme.md](../services/homepi-usb-devices/services-homepi-usb-devices-install-readme.md) — USB service detail
