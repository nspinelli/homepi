# HomePi Feature Spec: Contact Sensors

## 1. Module Name

Service name:

```txt
homepi-sensors
```

Public command socket:

```txt
/run/homepi/sensors/sensors.sock
```

> **Architecture v2 (2026):** Contact Sensors is the `homepi-sensors` module facade at `sensors.sock`. Commands: `UI → homepi-backend REST → sensors.sock`. Events publish to `homepi-broker` (`homepi.sensors.*`). HomeKit exposure uses the shared platform bridge `homepi-homekit` (see [homekit-bridge.md](./architecture/homekit-bridge.md)).

Feature path:

```txt
/contact-sensors
```

Service package:

```txt
services/sensors/homepi-sensors/
```

This module manages the HomePi custom contact-sensor Expansion HAT and exposes selected sensors to Apple Home through the **platform HomeKit bridge** (`homepi-homekit`).

This is a feature module, not a core service.

The module is responsible for:

- Reading contact sensors from the custom HomePi Expansion HAT.
- Managing two MCP23017 GPIO expanders.
- Managing direct Raspberry Pi GPIO contact sensors.
- Detecting sensor changes using interrupts and GPIO edge events.
- Persisting sensor configuration and current state.
- Supporting local HomePi room assignments.
- Supporting sensor classification as `door`, `window`, or `other`.
- Emitting normalized contact sensor events through `homepi-broker` (`homepi.sensors.*`).
- Handling direct command/response requests on `sensors.sock`.
- Syncing HomeKit-enabled sensors with the platform `homepi-homekit` bridge.

This module must be event-driven only.

No polling loops are allowed.

This module must not expose additional public sockets beyond the module facade at `sensors.sock`.

---

## 2. Core Integration Model (v2)

The module integrates with HomePi v2 platform services.

| Service | Responsibility |
|---------|----------------|
| `@homepi/core-logging` | Structured service logging |
| SQLite (`/opt/homepi/runtime/state/homepi.sqlite`) | Sensor config and state persistence |
| `homepi-broker` | Event fanout (`homepi.sensors.*` topics) |
| `homepi-backend` | REST API (`/api/contact-sensors`) and SSE to UI |
| `homepi-health` | Module/capability health rollups |
| `homepi-homekit` | Platform HomeKit bridge (accessory register/update/remove) |

### 2.1 Command flow

```txt
UI
  -> homepi-backend (REST)
  -> /run/homepi/sensors/sensors.sock (homepi-sensors)
  -> response
```

### 2.2 Event flow

```txt
homepi-sensors
  -> homepi-broker publish (homepi.sensors.contact.changed, etc.)
  -> homepi-backend EventsBrokerBridge
  -> SSE /events
  -> UI
```

### 2.3 No extra public sockets

Do not create `/run/homepi/contact-sensors.sock`. The only public module socket is `/run/homepi/sensors/sensors.sock`.

---

## 3. Event-Driven Requirement

The service must not poll sensor state on an interval.

Do not use:

```ts
setInterval(...)
```

Do not repeatedly scan all sensors.

Do not periodically query MCP23017 GPIO registers.

Do not periodically read Raspberry Pi GPIO states.

The only valid runtime triggers are:

- MCP23017 interrupt lines.
- Raspberry Pi GPIO edge events.
- User configuration changes routed through `core/broker`.
- Service startup initialization.
- Explicit debug or diagnostic commands routed through `core/broker`.

A startup state read is allowed because the service must establish initial state.

A one-shot debounce timer after an interrupt or edge event is allowed because it is part of event handling, not polling.

Broker-routed diagnostic requests may perform explicit one-time reads, but they must not create a polling loop.

---

## 4. Hardware Layout

The custom Expansion HAT supports 38 total contact sensors.

| Sensor Range | Hardware | Count |
|---|---:|---:|
| Sensor 1-16 | MCP23017 #1 | 16 |
| Sensor 17-32 | MCP23017 #2 | 16 |
| Sensor 33-38 | Raspberry Pi GPIO | 6 |

Important schematic naming note:

```txt
GPIO_33 through GPIO_38 in the schematic are logical contact sensor nets.
They are not Raspberry Pi BCM GPIO numbers.
```

Example:

```txt
Schematic net GPIO_38 = Contact Sensor 38
Raspberry Pi BCM GPIO = 26
Raspberry Pi physical pin = 37
```

---

## 5. Raspberry Pi I2C Pins

The MCP23017 expanders use Raspberry Pi I2C bus 1.

| Function | Raspberry Pi Physical Pin | BCM GPIO |
|---|---:|---:|
| SDA | Pin 3 | BCM 2 |
| SCL | Pin 5 | BCM 3 |

The service should use:

```txt
/dev/i2c-1
```

---

## 6. MCP23017 Devices

| MCP Device | I2C Address | Sensor Range |
|---|---:|---:|
| MCP1 / IC1 | `0x20` | Sensor 1-16 |
| MCP2 / IC2 | `0x21` | Sensor 17-32 |

Address configuration from schematic:

| MCP Device | A0 | A1 | A2 | Address |
|---|---|---|---|---:|
| MCP1 / IC1 | GND | GND | GND | `0x20` |
| MCP2 / IC2 | VDD | GND | GND | `0x21` |

### 6.1 MCP GPIO Setup

Each MCP23017 pin must be configured as an input.

Expected register setup:

```txt
IODIRA = 0xFF
IODIRB = 0xFF
GPPUA  = 0xFF
GPPUB  = 0xFF
```

This means:

- Bank A pins are inputs.
- Bank B pins are inputs.
- Internal pull-ups are enabled.
- Contact closure should pull the pin low.
- Raw value `0` means closed.
- Raw value `1` means open.

If the HAT wiring is later changed, the inversion behavior must be configurable per sensor.

### 6.2 MCP23017 Interrupt Pins

Final interrupt mapping from the schematic:

| MCP Device | Interrupt | Schematic Net | Raspberry Pi Physical Pin | BCM GPIO |
|---|---|---|---:|---:|
| MCP1 / IC1 | INTA | `IC1_INTA` | Pin 16 | BCM 23 |
| MCP1 / IC1 | INTB | `IC1_INTB` | Pin 18 | BCM 24 |
| MCP2 / IC2 | INTA | `IC2_INTA` | Pin 22 | BCM 25 |
| MCP2 / IC2 | INTB | `IC2_INTB` | Pin 36 | BCM 16 |

The contact sensor service must monitor these four interrupt lines using `libgpiod`.

No polling is allowed.

When an MCP interrupt fires:

1. Identify the expander and bank.
2. Read the interrupt capture register where applicable.
3. Read the relevant GPIO bank state.
4. Normalize changed pins into sensor state changes.
5. Emit events only for sensors whose state actually changed.
6. Persist updated state through `core/database`.
7. Publish the state change through `core/events`.
8. Update HomeKit if the sensor is enabled for HomeKit.

---

## 7. Direct Raspberry Pi GPIO Sensors

Sensors 33-38 are connected directly to Raspberry Pi GPIO pins.

Final direct GPIO sensor mapping from the schematic:

| Sensor Number | Schematic Net | Raspberry Pi Physical Pin | BCM GPIO |
|---:|---|---:|---:|
| 33 | `GPIO_33` | Pin 11 | BCM 17 |
| 34 | `GPIO_34` | Pin 13 | BCM 27 |
| 35 | `GPIO_35` | Pin 15 | BCM 22 |
| 36 | `GPIO_36` | Pin 29 | BCM 5 |
| 37 | `GPIO_37` | Pin 31 | BCM 6 |
| 38 | `GPIO_38` | Pin 37 | BCM 26 |

Each direct GPIO contact sensor must be monitored using edge events through `libgpiod`.

No polling is allowed.

---

## 8. MCP Sensor Pin Mapping

### 8.1 MCP1 / IC1 / Address `0x20`

| Sensor Number | MCP Bank | MCP Pin | Schematic Net |
|---:|---|---:|---|
| 1 | A | 0 | `GPIO_01` |
| 2 | A | 1 | `GPIO_02` |
| 3 | A | 2 | `GPIO_03` |
| 4 | A | 3 | `GPIO_04` |
| 5 | A | 4 | `GPIO_05` |
| 6 | A | 5 | `GPIO_06` |
| 7 | A | 6 | `GPIO_07` |
| 8 | A | 7 | `GPIO_08` |
| 9 | B | 0 | `GPIO_09` |
| 10 | B | 1 | `GPIO_10` |
| 11 | B | 2 | `GPIO_11` |
| 12 | B | 3 | `GPIO_12` |
| 13 | B | 4 | `GPIO_13` |
| 14 | B | 5 | `GPIO_14` |
| 15 | B | 6 | `GPIO_15` |
| 16 | B | 7 | `GPIO_16` |

### 8.2 MCP2 / IC2 / Address `0x21`

| Sensor Number | MCP Bank | MCP Pin | Schematic Net |
|---:|---|---:|---|
| 17 | A | 0 | `GPIO_17` |
| 18 | A | 1 | `GPIO_18` |
| 19 | A | 2 | `GPIO_19` |
| 20 | A | 3 | `GPIO_20` |
| 21 | A | 4 | `GPIO_21` |
| 22 | A | 5 | `GPIO_22` |
| 23 | A | 6 | `GPIO_23` |
| 24 | A | 7 | `GPIO_24` |
| 25 | B | 0 | `GPIO_25` |
| 26 | B | 1 | `GPIO_26` |
| 27 | B | 2 | `GPIO_27` |
| 28 | B | 3 | `GPIO_28` |
| 29 | B | 4 | `GPIO_29` |
| 30 | B | 5 | `GPIO_30` |
| 31 | B | 6 | `GPIO_31` |
| 32 | B | 7 | `GPIO_32` |

---

## 9. Sensor Classification

Each sensor must have a user-configurable type.

Field name:

```txt
sensor_type
```

Allowed values:

```txt
door
window
other
```

### 9.1 Meaning

| Sensor Type | Meaning |
|---|---|
| `door` | A contact sensor attached to a door |
| `window` | A contact sensor attached to a window |
| `other` | Any other contact-based sensor, cabinet, gate, access panel, attic hatch, garage contact, etc. |

The sensor type is a HomePi classification.

It should be used for:

- UI icons.
- UI grouping.
- Filtering.
- Default naming.
- Future automation conditions.
- Dashboard presentation.

The sensor type does not change hardware behavior.

All three types still use the same normalized contact states:

```txt
open
closed
unknown
```

Suggested UI icon mapping:

| Sensor Type | Suggested Icon |
|---|---|
| `door` | Door icon |
| `window` | Window icon |
| `other` | Generic contact sensor icon |

Use Lucide icons where possible to stay consistent with the rest of the HomePi UI.

---

## 10. Room Assignments

HomePi must support local room assignments.

Room assignments are owned by HomePi, not HomeKit.

Required fields:

```txt
room_id
room_name
```

HomePi rooms are used for:

- Dashboard grouping.
- Sensor filtering.
- Sensor cards.
- Local automations.
- Future module integrations.

HomeKit rooms are managed separately inside the Apple Home app.

Do not rely on HomeKit room changes syncing back into HomePi.

---

## 11. Sensor State Model

```ts
export type ContactState = "open" | "closed" | "unknown";

export type ContactSensorType = "door" | "window" | "other";

export interface ContactSensorState {
  sensorId: string;
  sensorNumber: number;
  name: string;

  roomId: string | null;
  roomName: string | null;

  sensorType: ContactSensorType;

  contactState: ContactState;
  isOpen: boolean | null;
  rawValue: number | null;

  homekitEnabled: boolean;

  lastChangedAt: string | null;
  lastSeenAt: string;

  faulted: boolean;
  faultReason: string | null;

  tamperSupported: boolean;
  tampered: boolean;
  tamperReason: string | null;
}
```

For the current HAT design:

| Raw Value | Contact State |
|---:|---|
| `0` | `closed` |
| `1` | `open` |

This must be configurable per sensor through `invert_logic`.

---

## 12. User Configuration

The user must be able to configure each sensor from the UI.

Required configurable fields:

```ts
export interface ContactSensorConfig {
  sensorId: string;
  sensorNumber: number;
  name: string;

  roomId: string | null;
  roomName: string | null;

  sensorType: "door" | "window" | "other";

  homekitEnabled: boolean;
  invertLogic: boolean;
  debounceMs: number;
}
```

### 12.1 HomeKit Enable / Disable Behavior

The user can enable or disable whether each sensor is exposed to HomeKit.

Field:

```txt
homekit_enabled
```

Behavior:

- If `homekit_enabled = true`, the sensor appears in HomeKit.
- If `homekit_enabled = false`, the sensor does not appear in HomeKit.
- The sensor still appears in HomePi when “show disabled” is enabled.
- The sensor is still monitored by HomePi even when not exposed to HomeKit.
- Disabling HomeKit exposure does not stop hardware monitoring.

This keeps the sensor available for:

- Dashboard display.
- Logs.
- Diagnostics.
- Future local automations.
- Future alarm/security features.

Do not add a separate `sensor_active` field in v1 unless the user needs to fully ignore a hardware input.

For v1:

```txt
homekit_enabled = controls Apple Home exposure only
```

The sensor remains active internally.

---

## 13. HomePi UI Behavior

The contact sensor UI should use the same general card philosophy as the Home Audio zone cards.

Each sensor card should show:

- Sensor name.
- Sensor type.
- Room assignment.
- Current state.
- HomeKit enabled/disabled status.
- Fault status if present.
- Hardware source.

Example enabled card:

```txt
Front Door
Type: Door
Room: Entry
State: Closed
HomeKit: Enabled
Hardware: MCP1 / Bank A / Pin 0
```

Example disabled-for-HomeKit card:

```txt
Garage Window
Type: Window
Room: Garage
State: Open
HomeKit: Disabled
Hardware: MCP2 / Bank B / Pin 3
```

### 13.1 Disabled Sensor Visibility

The UI should support a “Show Disabled” toggle.

Default recommendation:

```txt
Show Disabled: off
```

When off:

- Show sensors enabled for HomeKit.
- Hide sensors disabled for HomeKit.

When on:

- Show all sensors.
- Visually mute disabled-for-HomeKit cards.
- Continue showing current sensor state.

Disabled-for-HomeKit does not mean broken.

Do not use warning/error styling for disabled sensors.

### 13.2 Editing Sensor

The edit panel should allow:

- Name.
- Room.
- Sensor type.
- HomeKit enabled.
- Invert logic.
- Debounce duration.
- Diagnostic hardware details read-only.

Suggested edit fields:

```txt
Name: Front Door
Room: Entry
Type: Door / Window / Other
Expose to HomeKit: On / Off
Invert Logic: On / Off
Debounce: 0 ms
```

Hardware details should be visible but not normally editable:

```txt
Hardware: MCP1
I2C Address: 0x20
Bank: A
Pin: 0
```

---

## 14. Tamper and Fault

### 14.1 Fault

A fault means HomePi cannot confidently report the sensor state.

Examples:

- MCP23017 missing on I2C.
- MCP23017 read failed.
- GPIO line cannot be opened.
- Interrupt line unavailable.
- Sensor state is unknown after startup.
- Hardware mapping is invalid.

Fields:

```ts
faulted: boolean;
faultReason: string | null;
```

Allowed initial fault reasons:

```txt
mcp23017_missing
mcp23017_read_failed
gpio_line_unavailable
interrupt_line_unavailable
invalid_hardware_mapping
startup_state_unknown
homekit_update_failed
```

A fault should be shown clearly in the HomePi UI.

A fault should publish a `contact.sensor.faulted` event through `core/events`.

A fault may be reflected to HomeKit using `StatusFault` when supported.

### 14.2 Tamper

A tamper means the sensor may be physically compromised.

Examples:

- Wire cut.
- Sensor cover opened.
- Magnet removed suspiciously.
- Tamper loop opened.
- Supervised resistance state is invalid.

For the current HAT, tamper should be schema-ready but disabled unless the hardware has a dedicated tamper loop or supervised circuit.

A normal two-wire reed switch cannot reliably distinguish:

- Door/window opened normally.
- Wire cut.
- Magnet removed.
- Sensor removed.

V1 default values:

```txt
tamper_supported = false
tampered = false
tamper_reason = null
```

Fields:

```ts
tamperSupported: boolean;
tampered: boolean;
tamperReason: string | null;
```

Allowed initial tamper reasons:

```txt
tamper_loop_open
supervision_invalid
sensor_removed
wire_cut_detected
unknown_tamper
```

Do not show tamper warnings unless `tamper_supported = true`.

---

## 15. HomeKit Bridge (platform)

HomeKit support is provided by the shared platform service **`homepi-homekit`** ([homekit-bridge.md](./architecture/homekit-bridge.md)). This module does not embed HAP-NodeJS.

When `homekit_enabled` is true, `homepi-sensors` registers a `ContactSensor` accessory via `homekit.accessory.register` / `update` / `remove`.

### 15.1 Bridge

Bridge name:

```txt
HomePi Contact Sensors
```

Storage path:

```txt
data/homekit/contact-sensors
```

Category:

```txt
BRIDGE
```

The bridge identity must be stable across restarts.

Do not generate a new bridge identity on each boot.

The bridge MAC / username should be generated deterministically and stored.

### 15.2 Accessories

Each HomeKit-enabled sensor should be represented as a bridged accessory.

Accessory type:

```txt
ContactSensor
```

Primary service:

```txt
Service.ContactSensor
```

Primary characteristic:

```txt
Characteristic.ContactSensorState
```

State mapping:

| HomePi State | HomeKit State |
|---|---:|
| `closed` | `CONTACT_DETECTED` |
| `open` | `CONTACT_NOT_DETECTED` |
| `unknown` | use `StatusFault` |

### 15.3 Sensor Type and HomeKit

HomeKit exposure should not depend on `sensor_type`.

All HomePi sensor types still map to the HomeKit `ContactSensor` service:

| HomePi Sensor Type | HomeKit Service |
|---|---|
| `door` | `ContactSensor` |
| `window` | `ContactSensor` |
| `other` | `ContactSensor` |

The `sensor_type` field is mainly for HomePi UI, filtering, grouping, and future automations.

### 15.4 Accessory Stability

Each accessory must have a stable UUID.

Suggested stable UUID input:

```txt
homepi-contact-sensor-${sensor_number}
```

Examples:

```txt
homepi-contact-sensor-001
homepi-contact-sensor-002
homepi-contact-sensor-038
```

Do not base UUIDs on display names because the user may rename sensors.

### 15.5 Enabling a Sensor for HomeKit

When a user enables HomeKit for a sensor through the API:

```txt
UI/API
  -> core/api
  -> core/broker
  -> homepi-sensors
```

The feature must:

1. Update `homekit_enabled = true` in the database.
2. Create the HomeKit accessory if it does not already exist.
3. Add the accessory to the bridge.
4. Push the current known state to HomeKit immediately.
5. Publish `contact.sensor.homekit.enabled` through `core/events`.
6. Return a broker response to the original API request.

### 15.6 Disabling a Sensor for HomeKit

When a user disables HomeKit for a sensor through the API:

```txt
UI/API
  -> core/api
  -> core/broker
  -> homepi-sensors
```

The feature must:

1. Update `homekit_enabled = false` in the database.
2. Remove or unpublish the accessory from the HomeKit bridge.
3. Preserve the sensor config and current state in the database.
4. Continue monitoring hardware state.
5. Publish `contact.sensor.homekit.disabled` through `core/events`.
6. Return a broker response to the original API request.

The implementation must avoid creating duplicate accessories when a sensor is repeatedly enabled and disabled.

### 15.7 Renaming a Sensor

When a user renames a sensor in HomePi:

1. Update the HomePi database.
2. Update the HomePi UI.
3. If exposed to HomeKit, update the HomeKit accessory display name.
4. Preserve the same HomeKit UUID.

Do not create a new HomeKit accessory on rename.

### 15.8 Changing Sensor Type

When a user changes sensor type:

1. Update the HomePi database.
2. Update the HomePi UI card icon and classification.
3. Emit a config updated event.
4. Do not recreate the HomeKit accessory.
5. Optionally update the HomeKit display name only if the name is auto-generated.

Changing from `window` to `door` must not change the sensor ID or HomeKit UUID.

---

## 16. Database Schema

All persistence must go through HomePi core database/storage.

### 16.1 Table: `CONTACT_SENSOR_CONTROLLERS`

This table stores the MCP23017 controller-level hardware mapping, including interrupt pins.

```sql
CREATE TABLE IF NOT EXISTS CONTACT_SENSOR_CONTROLLERS (
  CONTROLLER_ID TEXT PRIMARY KEY,

  CONTROLLER_TYPE TEXT NOT NULL,
  CONTROLLER_MODEL TEXT NOT NULL,

  I2C_BUS TEXT,
  I2C_ADDRESS TEXT,

  INTA_NET_NAME TEXT,
  INTA_PI_PHYSICAL_PIN INTEGER,
  INTA_BCM_GPIO INTEGER,

  INTB_NET_NAME TEXT,
  INTB_PI_PHYSICAL_PIN INTEGER,
  INTB_BCM_GPIO INTEGER,

  CREATED_AT TEXT NOT NULL,
  UPDATED_AT TEXT NOT NULL,

  CHECK (CONTROLLER_TYPE IN ('mcp23017', 'raspberry_pi_gpio'))
);
```

Seed rows:

```json
[
  {
    "CONTROLLER_ID": "mcp1",
    "CONTROLLER_TYPE": "mcp23017",
    "CONTROLLER_MODEL": "MCP1",
    "I2C_BUS": "/dev/i2c-1",
    "I2C_ADDRESS": "0x20",
    "INTA_NET_NAME": "IC1_INTA",
    "INTA_PI_PHYSICAL_PIN": 16,
    "INTA_BCM_GPIO": 23,
    "INTB_NET_NAME": "IC1_INTB",
    "INTB_PI_PHYSICAL_PIN": 18,
    "INTB_BCM_GPIO": 24
  },
  {
    "CONTROLLER_ID": "mcp2",
    "CONTROLLER_TYPE": "mcp23017",
    "CONTROLLER_MODEL": "MCP2",
    "I2C_BUS": "/dev/i2c-1",
    "I2C_ADDRESS": "0x21",
    "INTA_NET_NAME": "IC2_INTA",
    "INTA_PI_PHYSICAL_PIN": 22,
    "INTA_BCM_GPIO": 25,
    "INTB_NET_NAME": "IC2_INTB",
    "INTB_PI_PHYSICAL_PIN": 36,
    "INTB_BCM_GPIO": 16
  },
  {
    "CONTROLLER_ID": "raspberry_pi",
    "CONTROLLER_TYPE": "raspberry_pi_gpio",
    "CONTROLLER_MODEL": "Raspberry Pi",
    "I2C_BUS": null,
    "I2C_ADDRESS": null,
    "INTA_NET_NAME": null,
    "INTA_PI_PHYSICAL_PIN": null,
    "INTA_BCM_GPIO": null,
    "INTB_NET_NAME": null,
    "INTB_PI_PHYSICAL_PIN": null,
    "INTB_BCM_GPIO": null
  }
]
```

### 16.2 Table: `CONTACT_SENSORS`

Updated sensor table:

```sql
CREATE TABLE IF NOT EXISTS CONTACT_SENSORS (
  SENSOR_ID TEXT PRIMARY KEY,
  SENSOR_NUMBER INTEGER NOT NULL UNIQUE,

  SENSOR_NAME TEXT NOT NULL,
  SENSOR_TYPE TEXT NOT NULL DEFAULT 'other',

  ROOM_ID TEXT,
  ROOM_NAME TEXT,

  CONTROLLER_ID TEXT NOT NULL,

  HARDWARE_TYPE TEXT NOT NULL,
  HARDWARE_MODEL TEXT NOT NULL,

  SCHEMATIC_NET_NAME TEXT,

  I2C_ADDRESS TEXT,
  MCP_BANK TEXT,
  MCP_PIN INTEGER,

  PI_PHYSICAL_PIN INTEGER,
  BCM_GPIO INTEGER,
  GPIO_CHIP TEXT,
  GPIO_LINE INTEGER,

  INVERT_LOGIC INTEGER NOT NULL DEFAULT 0,
  DEBOUNCE_MS INTEGER NOT NULL DEFAULT 0,

  HOMEKIT_ENABLED INTEGER NOT NULL DEFAULT 0,
  HOMEKIT_ACCESSORY_UUID TEXT,

  CONTACT_STATE TEXT NOT NULL DEFAULT 'unknown',
  RAW_VALUE INTEGER,

  FAULTED INTEGER NOT NULL DEFAULT 0,
  FAULT_REASON TEXT,

  TAMPER_SUPPORTED INTEGER NOT NULL DEFAULT 0,
  TAMPERED INTEGER NOT NULL DEFAULT 0,
  TAMPER_REASON TEXT,

  LAST_CHANGED_AT TEXT,
  LAST_SEEN_AT TEXT,

  CREATED_AT TEXT NOT NULL,
  UPDATED_AT TEXT NOT NULL,

  CHECK (SENSOR_TYPE IN ('door', 'window', 'other')),
  CHECK (HARDWARE_TYPE IN ('mcp23017', 'raspberry_pi_gpio')),
  CHECK (HARDWARE_MODEL IN ('MCP1', 'MCP2', 'Raspberry Pi')),
  CHECK (CONTACT_STATE IN ('open', 'closed', 'unknown')),

  FOREIGN KEY (CONTROLLER_ID)
    REFERENCES CONTACT_SENSOR_CONTROLLERS(CONTROLLER_ID)
);
```

### 16.3 Allowed Values

Allowed `HARDWARE_TYPE` values:

```txt
mcp23017
raspberry_pi_gpio
```

Allowed `HARDWARE_MODEL` values:

```txt
MCP1
MCP2
Raspberry Pi
```

Allowed `SENSOR_TYPE` values:

```txt
door
window
other
```

---

## 17. Seed Mapping: All 38 Sensors

### 17.1 Sensors 1-16: MCP1 / IC1 / `0x20`

```json
[
  { "sensor_number": 1, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_01", "i2c_address": "0x20", "mcp_bank": "A", "mcp_pin": 0 },
  { "sensor_number": 2, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_02", "i2c_address": "0x20", "mcp_bank": "A", "mcp_pin": 1 },
  { "sensor_number": 3, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_03", "i2c_address": "0x20", "mcp_bank": "A", "mcp_pin": 2 },
  { "sensor_number": 4, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_04", "i2c_address": "0x20", "mcp_bank": "A", "mcp_pin": 3 },
  { "sensor_number": 5, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_05", "i2c_address": "0x20", "mcp_bank": "A", "mcp_pin": 4 },
  { "sensor_number": 6, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_06", "i2c_address": "0x20", "mcp_bank": "A", "mcp_pin": 5 },
  { "sensor_number": 7, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_07", "i2c_address": "0x20", "mcp_bank": "A", "mcp_pin": 6 },
  { "sensor_number": 8, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_08", "i2c_address": "0x20", "mcp_bank": "A", "mcp_pin": 7 },
  { "sensor_number": 9, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_09", "i2c_address": "0x20", "mcp_bank": "B", "mcp_pin": 0 },
  { "sensor_number": 10, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_10", "i2c_address": "0x20", "mcp_bank": "B", "mcp_pin": 1 },
  { "sensor_number": 11, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_11", "i2c_address": "0x20", "mcp_bank": "B", "mcp_pin": 2 },
  { "sensor_number": 12, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_12", "i2c_address": "0x20", "mcp_bank": "B", "mcp_pin": 3 },
  { "sensor_number": 13, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_13", "i2c_address": "0x20", "mcp_bank": "B", "mcp_pin": 4 },
  { "sensor_number": 14, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_14", "i2c_address": "0x20", "mcp_bank": "B", "mcp_pin": 5 },
  { "sensor_number": 15, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_15", "i2c_address": "0x20", "mcp_bank": "B", "mcp_pin": 6 },
  { "sensor_number": 16, "controller_id": "mcp1", "hardware_type": "mcp23017", "hardware_model": "MCP1", "schematic_net_name": "GPIO_16", "i2c_address": "0x20", "mcp_bank": "B", "mcp_pin": 7 }
]
```

### 17.2 Sensors 17-32: MCP2 / IC2 / `0x21`

```json
[
  { "sensor_number": 17, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_17", "i2c_address": "0x21", "mcp_bank": "A", "mcp_pin": 0 },
  { "sensor_number": 18, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_18", "i2c_address": "0x21", "mcp_bank": "A", "mcp_pin": 1 },
  { "sensor_number": 19, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_19", "i2c_address": "0x21", "mcp_bank": "A", "mcp_pin": 2 },
  { "sensor_number": 20, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_20", "i2c_address": "0x21", "mcp_bank": "A", "mcp_pin": 3 },
  { "sensor_number": 21, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_21", "i2c_address": "0x21", "mcp_bank": "A", "mcp_pin": 4 },
  { "sensor_number": 22, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_22", "i2c_address": "0x21", "mcp_bank": "A", "mcp_pin": 5 },
  { "sensor_number": 23, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_23", "i2c_address": "0x21", "mcp_bank": "A", "mcp_pin": 6 },
  { "sensor_number": 24, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_24", "i2c_address": "0x21", "mcp_bank": "A", "mcp_pin": 7 },
  { "sensor_number": 25, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_25", "i2c_address": "0x21", "mcp_bank": "B", "mcp_pin": 0 },
  { "sensor_number": 26, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_26", "i2c_address": "0x21", "mcp_bank": "B", "mcp_pin": 1 },
  { "sensor_number": 27, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_27", "i2c_address": "0x21", "mcp_bank": "B", "mcp_pin": 2 },
  { "sensor_number": 28, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_28", "i2c_address": "0x21", "mcp_bank": "B", "mcp_pin": 3 },
  { "sensor_number": 29, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_29", "i2c_address": "0x21", "mcp_bank": "B", "mcp_pin": 4 },
  { "sensor_number": 30, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_30", "i2c_address": "0x21", "mcp_bank": "B", "mcp_pin": 5 },
  { "sensor_number": 31, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_31", "i2c_address": "0x21", "mcp_bank": "B", "mcp_pin": 6 },
  { "sensor_number": 32, "controller_id": "mcp2", "hardware_type": "mcp23017", "hardware_model": "MCP2", "schematic_net_name": "GPIO_32", "i2c_address": "0x21", "mcp_bank": "B", "mcp_pin": 7 }
]
```

### 17.3 Sensors 33-38: Direct Raspberry Pi GPIO

```json
[
  {
    "sensor_number": 33,
    "controller_id": "raspberry_pi",
    "hardware_type": "raspberry_pi_gpio",
    "hardware_model": "Raspberry Pi",
    "schematic_net_name": "GPIO_33",
    "pi_physical_pin": 11,
    "bcm_gpio": 17
  },
  {
    "sensor_number": 34,
    "controller_id": "raspberry_pi",
    "hardware_type": "raspberry_pi_gpio",
    "hardware_model": "Raspberry Pi",
    "schematic_net_name": "GPIO_34",
    "pi_physical_pin": 13,
    "bcm_gpio": 27
  },
  {
    "sensor_number": 35,
    "controller_id": "raspberry_pi",
    "hardware_type": "raspberry_pi_gpio",
    "hardware_model": "Raspberry Pi",
    "schematic_net_name": "GPIO_35",
    "pi_physical_pin": 15,
    "bcm_gpio": 22
  },
  {
    "sensor_number": 36,
    "controller_id": "raspberry_pi",
    "hardware_type": "raspberry_pi_gpio",
    "hardware_model": "Raspberry Pi",
    "schematic_net_name": "GPIO_36",
    "pi_physical_pin": 29,
    "bcm_gpio": 5
  },
  {
    "sensor_number": 37,
    "controller_id": "raspberry_pi",
    "hardware_type": "raspberry_pi_gpio",
    "hardware_model": "Raspberry Pi",
    "schematic_net_name": "GPIO_37",
    "pi_physical_pin": 31,
    "bcm_gpio": 6
  },
  {
    "sensor_number": 38,
    "controller_id": "raspberry_pi",
    "hardware_type": "raspberry_pi_gpio",
    "hardware_model": "Raspberry Pi",
    "schematic_net_name": "GPIO_38",
    "pi_physical_pin": 37,
    "bcm_gpio": 26
  }
]
```

---

## 18. TypeScript Hardware Maps

### 18.1 Controllers

```ts
export const CONTACT_SENSOR_CONTROLLERS = [
  {
    controllerId: "mcp1",
    controllerType: "mcp23017",
    controllerModel: "MCP1",
    i2cBus: "/dev/i2c-1",
    i2cAddress: "0x20",
    interrupts: {
      A: {
        netName: "IC1_INTA",
        piPhysicalPin: 16,
        bcmGpio: 23,
      },
      B: {
        netName: "IC1_INTB",
        piPhysicalPin: 18,
        bcmGpio: 24,
      },
    },
  },
  {
    controllerId: "mcp2",
    controllerType: "mcp23017",
    controllerModel: "MCP2",
    i2cBus: "/dev/i2c-1",
    i2cAddress: "0x21",
    interrupts: {
      A: {
        netName: "IC2_INTA",
        piPhysicalPin: 22,
        bcmGpio: 25,
      },
      B: {
        netName: "IC2_INTB",
        piPhysicalPin: 36,
        bcmGpio: 16,
      },
    },
  },
  {
    controllerId: "raspberry_pi",
    controllerType: "raspberry_pi_gpio",
    controllerModel: "Raspberry Pi",
  },
] as const;
```

### 18.2 Direct GPIO Sensor Map

```ts
export const DIRECT_GPIO_SENSOR_MAP = [
  {
    sensorNumber: 33,
    netName: "GPIO_33",
    piPhysicalPin: 11,
    bcmGpio: 17,
  },
  {
    sensorNumber: 34,
    netName: "GPIO_34",
    piPhysicalPin: 13,
    bcmGpio: 27,
  },
  {
    sensorNumber: 35,
    netName: "GPIO_35",
    piPhysicalPin: 15,
    bcmGpio: 22,
  },
  {
    sensorNumber: 36,
    netName: "GPIO_36",
    piPhysicalPin: 29,
    bcmGpio: 5,
  },
  {
    sensorNumber: 37,
    netName: "GPIO_37",
    piPhysicalPin: 31,
    bcmGpio: 6,
  },
  {
    sensorNumber: 38,
    netName: "GPIO_38",
    piPhysicalPin: 37,
    bcmGpio: 26,
  },
] as const;
```

---

## 19. Core Events

This feature must publish events through `core/events`.

Events are for state changes and notifications.

Events are not used for request/response API calls.

### 19.1 Sensor Changed Event

```json
{
  "type": "contact.sensor.changed",
  "source": "homepi-sensors",
  "sensor_id": "contact_001",
  "sensor_number": 1,
  "name": "Front Door",
  "sensor_type": "door",
  "room": {
    "room_id": "room_entry",
    "room_name": "Entry"
  },
  "hardware": {
    "type": "mcp23017",
    "model": "MCP1",
    "i2c_address": "0x20",
    "bank": "A",
    "pin": 0,
    "schematic_net_name": "GPIO_01"
  },
  "state": {
    "contact": "closed",
    "is_open": false,
    "raw_value": 0,
    "faulted": false,
    "fault_reason": null,
    "tamper_supported": false,
    "tampered": false,
    "tamper_reason": null
  },
  "timestamps": {
    "changed_at": "2026-06-22T22:00:00.000-04:00"
  }
}
```

### 19.2 Sensor Config Updated Event

```json
{
  "type": "contact.sensor.config.updated",
  "source": "homepi-sensors",
  "sensor_id": "contact_001",
  "sensor_number": 1,
  "changes": {
    "name": "Front Door",
    "sensor_type": "door",
    "room_id": "room_entry",
    "room_name": "Entry",
    "homekit_enabled": true,
    "invert_logic": false,
    "debounce_ms": 0
  },
  "timestamps": {
    "updated_at": "2026-06-22T22:00:00.000-04:00"
  }
}
```

### 19.3 HomeKit Enabled Event

```json
{
  "type": "contact.sensor.homekit.enabled",
  "source": "homepi-sensors",
  "sensor_id": "contact_001",
  "sensor_number": 1,
  "sensor_type": "door",
  "timestamps": {
    "updated_at": "2026-06-22T22:00:00.000-04:00"
  }
}
```

### 19.4 HomeKit Disabled Event

```json
{
  "type": "contact.sensor.homekit.disabled",
  "source": "homepi-sensors",
  "sensor_id": "contact_001",
  "sensor_number": 1,
  "sensor_type": "door",
  "timestamps": {
    "updated_at": "2026-06-22T22:00:00.000-04:00"
  }
}
```

### 19.5 Sensor Faulted Event

```json
{
  "type": "contact.sensor.faulted",
  "source": "homepi-sensors",
  "sensor_id": "contact_001",
  "sensor_number": 1,
  "sensor_type": "door",
  "fault": {
    "faulted": true,
    "fault_reason": "mcp23017_read_failed"
  },
  "timestamps": {
    "faulted_at": "2026-06-22T22:00:00.000-04:00"
  }
}
```

---

## 20. Broker Routing

The contact sensor feature must register request/response handlers with `core/broker`.

The feature does not own a socket.

The broker owns routing.

The API calls the broker.

The broker routes messages to the feature.

### 20.1 Service Registration

On startup, `homepi-sensors` must register itself with the broker.

Service registration example:

```json
{
  "type": "broker.service.register",
  "service_id": "homepi-sensors",
  "feature": "contact-sensors",
  "version": "1.0.0",
  "routes": [
    "contact.sensors.list",
    "contact.sensor.get",
    "contact.sensor.update",
    "contact.sensor.homekit.enable",
    "contact.sensor.homekit.disable",
    "contact.sensor.diagnostics.get"
  ]
}
```

The broker should know that these message types route to:

```txt
homepi-sensors
```

### 20.2 Request Envelope

All broker-routed requests should use a consistent envelope.

```ts
export interface BrokerRequest<TPayload = unknown> {
  type: string;
  requestId: string;
  source: string;
  target: string;
  payload: TPayload;
  timestamp: string;
}
```

Example:

```json
{
  "type": "contact.sensor.update",
  "requestId": "req_01JZ000000000000000001",
  "source": "homepi-api",
  "target": "homepi-sensors",
  "payload": {
    "sensor_id": "contact_001",
    "patch": {
      "name": "Front Door",
      "sensor_type": "door",
      "room_id": "room_entry",
      "room_name": "Entry",
      "homekit_enabled": true,
      "invert_logic": false,
      "debounce_ms": 0
    }
  },
  "timestamp": "2026-06-22T22:00:00.000-04:00"
}
```

### 20.3 Response Envelope

```ts
export interface BrokerResponse<TPayload = unknown> {
  type: string;
  requestId: string;
  source: string;
  target: string;
  ok: boolean;
  payload?: TPayload;
  error?: {
    code: string;
    message: string;
    details?: unknown;
  };
  timestamp: string;
}
```

Successful response:

```json
{
  "type": "contact.sensor.update.result",
  "requestId": "req_01JZ000000000000000001",
  "source": "homepi-sensors",
  "target": "homepi-api",
  "ok": true,
  "payload": {
    "sensor_id": "contact_001",
    "updated": true
  },
  "timestamp": "2026-06-22T22:00:00.000-04:00"
}
```

Error response:

```json
{
  "type": "contact.sensor.update.result",
  "requestId": "req_01JZ000000000000000001",
  "source": "homepi-sensors",
  "target": "homepi-api",
  "ok": false,
  "error": {
    "code": "invalid_sensor_type",
    "message": "sensor_type must be one of: door, window, other"
  },
  "timestamp": "2026-06-22T22:00:00.000-04:00"
}
```

---

## 21. Broker Message Types

### 21.1 List Sensors

Request type:

```txt
contact.sensors.list
```

Payload:

```json
{
  "include_disabled": true,
  "room_id": null,
  "sensor_type": null
}
```

Response type:

```txt
contact.sensors.list.result
```

Response payload:

```json
{
  "sensors": [
    {
      "sensor_id": "contact_001",
      "sensor_number": 1,
      "name": "Front Door",
      "sensor_type": "door",
      "room": {
        "room_id": "room_entry",
        "room_name": "Entry"
      },
      "contact_state": "closed",
      "is_open": false,
      "homekit_enabled": true,
      "faulted": false,
      "fault_reason": null,
      "tamper_supported": false,
      "tampered": false,
      "tamper_reason": null,
      "hardware": {
        "type": "mcp23017",
        "model": "MCP1",
        "i2c_address": "0x20",
        "bank": "A",
        "pin": 0,
        "schematic_net_name": "GPIO_01"
      },
      "last_changed_at": "2026-06-22T22:00:00.000-04:00"
    }
  ]
}
```

### 21.2 Get Sensor

Request type:

```txt
contact.sensor.get
```

Payload:

```json
{
  "sensor_id": "contact_001"
}
```

Response type:

```txt
contact.sensor.get.result
```

Response payload:

```json
{
  "sensor": {
    "sensor_id": "contact_001",
    "sensor_number": 1,
    "name": "Front Door",
    "sensor_type": "door",
    "room": {
      "room_id": "room_entry",
      "room_name": "Entry"
    },
    "contact_state": "closed",
    "is_open": false,
    "homekit_enabled": true,
    "faulted": false,
    "last_changed_at": "2026-06-22T22:00:00.000-04:00"
  }
}
```

### 21.3 Update Sensor

Request type:

```txt
contact.sensor.update
```

Payload:

```json
{
  "sensor_id": "contact_001",
  "patch": {
    "name": "Front Door",
    "sensor_type": "door",
    "room_id": "room_entry",
    "room_name": "Entry",
    "homekit_enabled": true,
    "invert_logic": false,
    "debounce_ms": 0
  }
}
```

Response type:

```txt
contact.sensor.update.result
```

Response payload:

```json
{
  "sensor_id": "contact_001",
  "updated": true
}
```

The update handler must publish `contact.sensor.config.updated` through `core/events` after a successful update.

If `homekit_enabled` changes, the handler must also publish either:

```txt
contact.sensor.homekit.enabled
```

or:

```txt
contact.sensor.homekit.disabled
```

### 21.4 Enable HomeKit

Request type:

```txt
contact.sensor.homekit.enable
```

Payload:

```json
{
  "sensor_id": "contact_001"
}
```

Response type:

```txt
contact.sensor.homekit.enable.result
```

Response payload:

```json
{
  "sensor_id": "contact_001",
  "homekit_enabled": true
}
```

### 21.5 Disable HomeKit

Request type:

```txt
contact.sensor.homekit.disable
```

Payload:

```json
{
  "sensor_id": "contact_001"
}
```

Response type:

```txt
contact.sensor.homekit.disable.result
```

Response payload:

```json
{
  "sensor_id": "contact_001",
  "homekit_enabled": false
}
```

### 21.6 Diagnostics

Request type:

```txt
contact.sensor.diagnostics.get
```

Payload:

```json
{
  "sensor_id": "contact_001"
}
```

Response type:

```txt
contact.sensor.diagnostics.get.result
```

Response payload:

```json
{
  "sensor_id": "contact_001",
  "hardware": {
    "type": "mcp23017",
    "model": "MCP1",
    "i2c_address": "0x20",
    "bank": "A",
    "pin": 0,
    "schematic_net_name": "GPIO_01"
  },
  "state": {
    "contact_state": "closed",
    "raw_value": 0,
    "last_changed_at": "2026-06-22T22:00:00.000-04:00",
    "last_seen_at": "2026-06-22T22:00:00.000-04:00"
  },
  "fault": {
    "faulted": false,
    "fault_reason": null
  },
  "tamper": {
    "tamper_supported": false,
    "tampered": false,
    "tamper_reason": null
  }
}
```

---

## 22. API Interaction

The public API must not call `homepi-sensors` directly.

The API must send broker requests.

Example API route:

```txt
GET /api/contact-sensors
```

Internal behavior:

```txt
core/api receives HTTP request
core/api creates broker request: contact.sensors.list
core/broker routes to homepi-sensors
homepi-sensors returns broker response
core/api returns HTTP response
```

Example API route:

```txt
PATCH /api/contact-sensors/contact_001
```

Internal behavior:

```txt
core/api receives HTTP patch
core/api validates basic API shape
core/api creates broker request: contact.sensor.update
core/broker routes to homepi-sensors
homepi-sensors validates domain schema
homepi-sensors updates database
homepi-sensors updates HomeKit if needed
homepi-sensors publishes events
homepi-sensors returns broker response
core/api returns HTTP response
```

---

## 23. Schema Validation

Broker messages must be schema validated.

Validation should happen at two layers:

1. `core/broker` validates the broker envelope.
2. `homepi-sensors` validates the domain payload.

Allowed sensor types:

```ts
const sensorTypeValues = ["door", "window", "other"] as const;
```

Allowed contact states:

```ts
const contactStateValues = ["open", "closed", "unknown"] as const;
```

Update validation:

```txt
sensor_type must be one of: door, window, other
homekit_enabled must be boolean
invert_logic must be boolean
debounce_ms must be integer >= 0 and <= 5000
```

Invalid update payload:

```json
{
  "sensor_id": "contact_001",
  "patch": {
    "sensor_type": "garage"
  }
}
```

Required broker response:

```json
{
  "type": "contact.sensor.update.result",
  "ok": false,
  "error": {
    "code": "invalid_sensor_type",
    "message": "sensor_type must be one of: door, window, other"
  }
}
```

---

## 24. Runtime Flow

### 24.1 Startup

On startup:

1. Initialize core logger.
2. Connect to `core/database`.
3. Connect to `core/events`.
4. Connect to `core/broker`.
5. Register contact sensor broker routes.
6. Load or seed all 38 sensor definitions.
7. Initialize MCP23017 devices.
8. Configure MCP GPIO direction, pull-ups, and interrupts.
9. Read current MCP GPIO state once.
10. Initialize Raspberry Pi GPIO edge watchers.
11. Read current direct GPIO state once.
12. Update database with initial state.
13. Start HomeKit bridge.
14. Add only sensors where `homekit_enabled = true`.
15. Publish `contact.service.ready` through `core/events`.

Startup reads are allowed because they establish initial state.

Startup must not become a continuous poll.

### 24.2 MCP Interrupt Flow

When an MCP interrupt GPIO edge is received:

1. Log the interrupt line.
2. Resolve MCP device and bank.
3. Read interrupt state from the MCP.
4. Read the current GPIO state for the affected bank.
5. Compare against cached previous state.
6. Normalize each changed pin into a contact state.
7. Update the database.
8. Publish `contact.sensor.changed` through `core/events`.
9. Update HomeKit if `homekit_enabled = true`.
10. Acknowledge and clear MCP interrupt state.

### 24.3 Direct GPIO Flow

When a direct Raspberry Pi GPIO edge is received:

1. Log the GPIO edge.
2. Resolve the GPIO line to a sensor.
3. Read the GPIO line value.
4. Normalize the raw value into `open` or `closed`.
5. Compare against cached previous state.
6. Update the database only if the state changed.
7. Publish `contact.sensor.changed` through `core/events`.
8. Update HomeKit if `homekit_enabled = true`.

### 24.4 Broker Command Flow

When a broker command is received:

1. Validate the request envelope.
2. Validate the domain payload.
3. Execute the command.
4. Persist changes through `core/database` when needed.
5. Update HomeKit when needed.
6. Publish related events through `core/events`.
7. Return a broker response with the original `requestId`.

---

## 25. Logging

All logs must go through `core/logging`.

Required log categories:

```txt
contact.service
contact.hardware
contact.mcp23017
contact.gpio
contact.homekit
contact.config
contact.broker
contact.events
contact.ui
```

### 25.1 Important Logs

Startup:

```json
{
  "level": "info",
  "service": "homepi-sensors",
  "category": "contact.service",
  "message": "Contact sensor service starting"
}
```

Broker registration:

```json
{
  "level": "info",
  "service": "homepi-sensors",
  "category": "contact.broker",
  "message": "Registered broker routes",
  "routes": [
    "contact.sensors.list",
    "contact.sensor.get",
    "contact.sensor.update",
    "contact.sensor.homekit.enable",
    "contact.sensor.homekit.disable",
    "contact.sensor.diagnostics.get"
  ]
}
```

MCP initialized:

```json
{
  "level": "info",
  "service": "homepi-sensors",
  "category": "contact.mcp23017",
  "message": "MCP23017 initialized",
  "controller_id": "mcp1",
  "address": "0x20",
  "model": "MCP1"
}
```

Sensor changed:

```json
{
  "level": "info",
  "service": "homepi-sensors",
  "category": "contact.hardware",
  "message": "Contact sensor state changed",
  "sensor_id": "contact_001",
  "sensor_number": 1,
  "sensor_type": "door",
  "state": "closed",
  "raw_value": 0
}
```

Broker request handled:

```json
{
  "level": "debug",
  "service": "homepi-sensors",
  "category": "contact.broker",
  "message": "Handled broker request",
  "request_id": "req_01JZ000000000000000001",
  "type": "contact.sensor.update",
  "ok": true
}
```

HomeKit update:

```json
{
  "level": "debug",
  "service": "homepi-sensors",
  "category": "contact.homekit",
  "message": "HomeKit contact sensor updated",
  "sensor_id": "contact_001",
  "homekit_state": "CONTACT_DETECTED"
}
```

MCP read error:

```json
{
  "level": "error",
  "service": "homepi-sensors",
  "category": "contact.mcp23017",
  "message": "Failed to read MCP23017 bank state",
  "address": "0x20",
  "bank": "A",
  "error": "..."
}
```

---

## 26. Systemd Service

Service name:

```txt
homepi-sensors.service
```

Example:

```ini
[Unit]
Description=HomePi Contact Sensors
After=network.target
Requires=homepi-database.service homepi-events.service homepi-broker.service

[Service]
Type=simple
User=homepi
Group=homepi
WorkingDirectory=/opt/homepi
ExecStart=/usr/bin/node /opt/homepi/features/contact-sensors/dist/index.js
Restart=on-failure
RestartSec=2

SupplementaryGroups=gpio i2c

NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full
ProtectHome=true
ReadWritePaths=/opt/homepi/data /run/homepi

[Install]
WantedBy=multi-user.target
```

The service must have access to:

- I2C bus.
- GPIO character devices.
- HomePi data directory.
- HomePi broker transport.
- HomePi events transport.

The service must not create its own public socket.

---

## 27. Error Handling

The service must keep running if one sensor fails.

Failure handling rules:

- If one MCP23017 is missing, mark sensors for that MCP as `unknown`.
- If one GPIO line cannot be opened, mark that sensor as `unknown`.
- If HomeKit fails to start, continue tracking hardware state and log the HomeKit failure.
- If database writes fail, log the error and keep the in-memory state updated.
- If broker registration fails, log the error and retry using the broker client reconnect policy.
- If an MCP interrupt fires but no state changed, log at debug level and do not publish a sensor changed event.
- If a user tries to set an invalid `sensor_type`, reject the broker request and leave the current config unchanged.

---

## 28. Performance Requirements

The service must be lightweight enough to run continuously on a Raspberry Pi.

Requirements:

- No polling loops.
- No repeated full sensor scans during normal operation.
- No feature-owned socket.
- Use `core/broker` for request/response traffic.
- Use `core/events` for published state changes.
- Keep one cached state object per sensor.
- Only write to the database when state or configuration changes.
- Only update HomeKit when a HomeKit-enabled sensor changes.
- Only publish events when normalized state changes.
- Use structured logging.
- Avoid noisy logs during normal stable operation.

---

## 29. Suggested Folder Structure

```txt
features/contact-sensors/
  package.json
  tsconfig.json
  src/
    index.ts
    ContactSensorService.ts

    config/
      sensorMap.ts

    hardware/
      Mcp23017Device.ts
      Mcp23017Manager.ts
      GpioLineWatcher.ts
      ContactStateNormalizer.ts

    homekit/
      ContactHomeKitBridge.ts
      ContactAccessoryFactory.ts

    storage/
      ContactSensorRepository.ts

    broker/
      ContactSensorBrokerRoutes.ts
      ContactSensorBrokerSchemas.ts
      ContactSensorBrokerHandlers.ts

    events/
      ContactSensorEvents.ts

    ui/
      contactSensorViewModel.ts

    types/
      contactSensor.types.ts
```

---

## 30. Implementation Notes for Cursor

### 30.1 Must-Have Rules

- Do not create a `contact-sensors.sock` socket.
- Do not poll sensors.
- Do not use `setInterval` for state scanning.
- Use `core/broker` for request/response traffic.
- Use `core/events` for state change publication.
- Use `core/database` for persistence.
- Use `core/logging` for all logs.
- Use `libgpiod` edge events for Raspberry Pi GPIO and MCP interrupt lines.
- Read MCP GPIO state only on startup or when an MCP interrupt fires.
- Read direct Raspberry Pi GPIO state only on startup or when a direct GPIO edge fires.
- HomeKit must be built into the feature as a bridge.
- `homekit_enabled` controls Apple Home exposure only; it does not disable internal monitoring.

### 30.2 Runtime GPIO Lines

MCP interrupt lines:

```txt
BCM 23 -> MCP1 INTA
BCM 24 -> MCP1 INTB
BCM 25 -> MCP2 INTA
BCM 16 -> MCP2 INTB
```

Direct sensor lines:

```txt
BCM 17 -> Sensor 33
BCM 27 -> Sensor 34
BCM 22 -> Sensor 35
BCM 5  -> Sensor 36
BCM 6  -> Sensor 37
BCM 26 -> Sensor 38
```

### 30.3 V1 Scope

The v1 implementation should focus on:

- 38 hardware-backed contact sensors.
- Correct Expansion HAT GPIO and interrupt mappings.
- Door/window/other classification.
- Local HomePi room assignment.
- Event-driven state updates.
- Persistent sensor config.
- Built-in HomeKit bridge.
- User-controlled HomeKit exposure per sensor.
- Disabled-for-HomeKit sensors visible in HomePi when requested.
- Broker-routed API commands.
- Core event publication for state changes.

