# Installing homepi-hifi-serial

## Prerequisites

- `homepi-usb-devices` installed with serial device assigned
- `/dev/vHifi` symlink present after udev rules deployed
- User `homepi` in group `dialout`

## Install

```bash
sudo bash services/homepi-hifi-serial/scripts/install.sh
```

## Verify

```bash
systemctl status homepi-hifi-serial
printf '%s\n' '{"method":"getHealth","correlationId":"verify"}' | nc -U /run/homepi/hifi-serial.sock
curl -s http://127.0.0.1:3000/api/hifi-serial/health
```

## Uninstall

```bash
sudo bash services/homepi-hifi-serial/scripts/uninstall.sh
```

## Notes

- Changing USB serial assignment requires restarting `homepi-hifi-serial`.
- Full sync runs on service start and via `POST /api/hifi-serial/sync`.
