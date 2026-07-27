# HomePi Platform HomeKit Bridge

Architecture v2 platform service for Apple Home integration. Modules register accessories; the bridge owns pairing, HAP-NodeJS, and persistence.

## Service

| Item | Value |
|------|-------|
| systemd unit | `homepi-homekit.service` |
| Socket | `/run/homepi/homekit/homekit.sock` |
| Package | `services/homekit/homepi-homekit` |
| Pairing storage | `/opt/homepi/runtime/state/homekit/` |

## Responsibilities

- Run one HAP bridge (`BRIDGE` category) with stable identity across reboots
- Accept accessory register/update/remove commands from module facades
- Persist pairing data and accessory UUIDs
- Publish broker events on `homepi.homekit.*` topics
- Expose health via `getHealth` / `homekit.bridge.getHealth`

## Command API (`@homepi/core-messaging`)

| Command | Purpose |
|---------|---------|
| `ping` | Liveness |
| `getHealth` / `homekit.bridge.getHealth` | Bridge status for `homepi-health` |
| `homekit.accessory.register` | Add bridged accessory (`moduleId`, `accessoryType`, `stableUuid`, `displayName`, initial state) |
| `homekit.accessory.update` | Push state/name changes |
| `homekit.accessory.remove` | Unpublish accessory from Home |
| `homekit.accessory.list` | Registered accessories (diagnostics) |

## Module integration contract

Modules (e.g. `homepi-sensors`) call the bridge over `homekit.sock`; they do **not** embed HAP-NodeJS.

```text
homepi-sensors (contact sensor state change)
  -> homekit.accessory.update when homekit_enabled
  -> homepi-broker (homepi.sensors.contact.*) for UI
```

Contact Sensors is the first consumer (`accessoryType: ContactSensor`). Future modules add other HAP service types through the same API.

## Broker topics

- `homepi.homekit.bridge.ready`
- `homepi.homekit.accessory.updated`
- `homepi.homekit.accessory.removed`

## Health

`homepi-health` probes `/run/homepi/homekit/homekit.sock`. Contact Sensors module capability `homekit-bridge` reflects bridge reachability when the platform service is registered.
