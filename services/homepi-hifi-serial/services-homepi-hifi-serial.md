# HomePi HiFi Serial

Native C++ service for Hi-Fi2 by HAI RS-232 control. Serial path comes from
[homepi-usb-devices](../homepi-usb-devices/) assignments (`/dev/vHifi`).

## Requirements

- Full [HIFI2 protocol](../../docs/HIFI2_PROTOCOL.pdf)
- 50 ms minimum spacing between transmitted commands
- Real-time SQLite updates from unsolicited `#` responses
- Full bulk sync on startup and via `syncController` / `POST /api/hifi-serial/sync`
- Core event envelopes on `/run/homepi/hifi-serial.sock`

## Data model

- **Controller** — version, network, page state
- **Zones** — 16 zones (names, EQ, power, volume, source, group)
- **Sources** — 8 sources
- **Groups** — 8 groups
- **Language strings** — indices 0–100

Persistence: `002-hifi-serial.sql` in shared `homepi.sqlite`.
