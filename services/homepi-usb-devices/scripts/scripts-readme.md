# homepi-usb-devices scripts

## Install

```bash
sudo bash services/homepi-usb-devices/scripts/install.sh
```

Builds the C++ daemon, installs to `/opt/homepi/services/usb-devices`, enables `homepi-usb-devices.service`, and restarts `homepi-backend`.

## Uninstall

```bash
sudo bash services/homepi-usb-devices/scripts/uninstall.sh
```

Stops and removes the systemd unit and install tree. Runtime SQLite and generated files are left in place.
