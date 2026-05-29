# homepi-usb-devices — Install and validation

## Install (recommended)

```bash
sudo bash /home/homepi/homepi/services/homepi-usb-devices/scripts/install.sh
```

## Uninstall

```bash
sudo bash /home/homepi/homepi/services/homepi-usb-devices/scripts/uninstall.sh
```

## Manual build (optional)

```bash
cd /home/homepi/homepi/services/homepi-usb-devices
mkdir -p build && cd build
cmake ..
cmake --build .
```

Dependencies: `libudev-dev`, `libsqlite3-dev`, `cmake`, `g++`.

## Install layout

```text
/opt/homepi/services/usb-devices/
├─ bin/homepi-usb-devices
├─ config/service-config.json
└─ storage/migrations/001-usb-devices.sql
```

Copy the built binary, config, and migrations. Ensure `/opt/homepi/runtime/state` and `/run/homepi` exist and are owned by `homepi`.

## systemd

```bash
sudo cp systemd/homepi-usb-devices.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now homepi-usb-devices.service
```

## Automatic deploy after save

Saving USB role assignments in the UI runs `post-assignment-hook.sh` automatically (udev deploy + `homepi-hifi-serial` restart). Requires the install script’s sudoers drop-in under `/etc/sudoers.d/homepi-usb-post-assignment`.

## udev rules activation (manual)

`install.sh` deploys rules automatically when generated rules exist. After changing the serial assignment in the UI, run:

```bash
sudo bash /opt/homepi/services/usb-devices/scripts/deploy-udev-rules.sh
```

Or from the repo: `sudo bash services/homepi-usb-devices/scripts/deploy-udev-rules.sh`

Verify serial symlink: `ls -l /dev/vHifi` (should point at your FTDI `ttyUSB` device)

## ALSA aliases

Generated snippets:

- `/opt/homepi/runtime/generated/alsa/10-homepi-audio-out.conf`
- `/opt/homepi/runtime/generated/alsa/20-homepi-audio-paging.conf`

List PCM devices: `aplay -L | grep -E 'AudioOut|AudioPaging'`

Set `ALSA_CONFIG_PATH` to include the generated `alsa` directory for consumer services.

## API (via backend proxy)

| Method | Path |
|--------|------|
| GET | `/api/usb-devices` |
| GET | `/api/usb-devices/assignments` |
| PUT | `/api/usb-devices/assignments` |
| GET | `/api/usb-devices/health` |

## Manual test checklist

1. Start `homepi-usb-devices` and confirm socket: `ls -l /run/homepi/usb-devices.sock`
2. Start backend; confirm status page shows **USB Devices** card as healthy
3. Open Settings → **Audio Configuration**; verify dropdowns list connected devices
4. Assign three different devices and click **Save**
5. Confirm DB row: `sqlite3 /opt/homepi/runtime/state/homepi.sqlite 'SELECT * FROM usb_assignments'`
6. Confirm `/dev/vHifi` and ALSA config files exist under `runtime/generated`
7. Unplug an assigned device; status should show **degraded**; replug restores **healthy**
