# HomePi USB Devices Core Service


## Service Name

```text
homepi-usb-devices
```


The systemd service and binary should use:

```text
homepi-usb-devices
```

---

# Purpose

`homepi-usb-devices` is a HomePi core infrastructure service responsible for detecting, classifying, tracking, and persistently mapping USB devices connected to the Raspberry Pi.

This service belongs in the HomePi `core` folder because USB device identity is shared infrastructure used by multiple services.

Existing core services:

```text
/core/logging
/core/storage
/core/usb
```

The USB service must use the existing core services for logging and persistence.

It must not create a separate logging system or write directly to the database.

---

# Primary Responsibilities

`homepi-usb-devices` is responsible for:

```text
Detecting USB device connect events
Detecting USB device disconnect events
Building a USB inventory snapshot at startup
Classifying USB devices
Detecting USB audio DACs
Detecting USB serial adapters
Detecting USB storage devices
Detecting USB HID devices
Detecting USB network devices
Reading ALSA capabilities for audio devices
Reading serial adapter metadata
Creating stable HomePi device IDs
Persisting USB device inventory through core/storage
Publishing USB device lifecycle events
Creating udev rules for user-selected device mappings
Supporting persistent ALSA aliases for selected audio DACs
```

---

# Non-Responsibilities

`homepi-usb-devices` must not:

```text
Open serial ports for HiFi2 communication
Send HiFi2 commands
Stream audio
Route PCM audio
Play paging audio
Own module-specific behavior
Directly write to SQLite
Directly write custom logs outside core/logging
Poll USB devices repeatedly
Depend on /dev/ttyUSB0 as a persistent identity
Depend on ALSA card numbers as persistent identity
```

---

# Core Dependencies

## Required Core Services

```text
/core/logging
/core/storage
```

Optional future dependency:

```text
/core/events
```

---

## Logging Requirement

All logs must go through `/core/logging`.

Do not use:

```cpp
std::cout
printf
custom file logging
direct syslog writes
```

Acceptable logging pattern:

```cpp
LOG_INFO("USB device connected", {
  {"device_stable_id", device.deviceStableId},
  {"vendor_id", device.vendorId},
  {"product_id", device.productId}
});

LOG_WARN("USB audio device has no unique serial number", {
  {"device_stable_id", device.deviceStableId},
  {"persistence_mode", "port_based"}
});

LOG_ERROR("Failed to probe ALSA device", {
  {"device_stable_id", device.deviceStableId},
  {"error", errorMessage}
});
```

---

## Storage Requirement

All persistence must go through `/core/storage`.

`homepi-usb-devices` must not directly open SQLite.

The storage layer should own:

```text
table creation
migrations
upserts
queries
transactions
```

The USB module should call storage APIs only.

---

# Runtime Model

The service should be event-driven.

Use Linux `udev` monitoring to detect:

```text
add
remove
change
```

The service must also perform a full USB snapshot on startup so HomePi knows what is already connected before hotplug monitoring begins.

---

# Implementation Language

Preferred implementation:

```text
C++
```

Reason:

```text
Low overhead
Fast startup
Efficient udev monitoring
Direct ALSA probing
Good fit for Raspberry Pi services
```

---

# External Libraries

Use:

```text
libudev
ALSA libasound
nlohmann/json or existing HomePi JSON utility
```

Do not shell out to:

```text
lsusb
aplay
arecord
udevadm
cat
grep
awk
```

Shell commands may be useful during manual debugging but should not be used in production service logic.

---

# Folder Structure

```text
/core/usb
├── README.md
├── CMakeLists.txt
├── systemd
│   └── homepi-usb-devices.service
├── include
│   └── homepi
│       └── core
│           └── usb
│               ├── usb_device.hpp
│               ├── usb_audio_capability.hpp
│               ├── usb_serial_capability.hpp
│               ├── usb_assignment.hpp
│               ├── usb_monitor.hpp
│               ├── usb_classifier.hpp
│               ├── alsa_probe.hpp
│               ├── serial_probe.hpp
│               ├── stable_id.hpp
│               ├── udev_rule_manager.hpp
│               ├── usb_registry.hpp
│               └── usb_service.hpp
├── src
│   ├── main.cpp
│   ├── usb_monitor.cpp
│   ├── usb_classifier.cpp
│   ├── alsa_probe.cpp
│   ├── serial_probe.cpp
│   ├── stable_id.cpp
│   ├── udev_rule_manager.cpp
│   ├── usb_registry.cpp
│   └── usb_service.cpp
└── tests
    ├── stable_id_tests.cpp
    ├── classifier_tests.cpp
    └── udev_rule_tests.cpp
```

---

# Device Types

A USB device may have one or more types.

Supported device types:

```text
audio_dac
serial
storage
hid
network
camera
unknown
```

Example:

```json
{
  "device_types": ["audio_dac"]
}
```

---

# Stable Device Identity

Every USB device must receive a HomePi stable ID.

Field name:

```text
DEVICE_STABLE_ID
```

This is the ID that other HomePi services should store.

Other services must not store:

```text
/dev/ttyUSB0
hw:2,0
ALSA card number
USB bus number
temporary devpath
```

---

# Stable ID Strategy

## Priority 1: Unique Serial Number

If the USB device exposes a unique serial number:

```text
vendor_id
product_id
serial_number
```

Generate:

```text
audio.05ac.110a.serial-00000001
serial.0403.6001.serial-a50285bi
```

---

## Priority 2: Existing Serial by-id Path

For serial devices, prefer `/dev/serial/by-id/*` when available.

Example:

```text
/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A50285BI-if00-port0
```

Generated stable ID:

```text
serial.ftdi.a50285bi
```

---

## Priority 3: Port-Based Identity

Some USB audio DACs do not expose unique serial numbers.

If two devices have the same:

```text
vendor_id
product_id
manufacturer
product_name
serial_number
```

then HomePi cannot distinguish the physical devices reliably.

In this case, use USB topology path.

Example:

```text
audio.1234.5678.port-1-1.2
audio.1234.5678.port-1-1.3
```

This means the assignment follows the USB port, not the physical DAC.

---

# Persistence Modes

Each USB device must include a persistence mode.

Supported values:

```text
serial_based
by_id_based
port_based
unknown
```

Example:

```json
{
  "device_stable_id": "audio.1234.5678.port-1-1.2",
  "persistence_mode": "port_based"
}
```

If a device is `port_based`, the UI must warn the user:

```text
This device does not expose a unique serial number.
HomePi will persist this assignment by USB port.
Keep this device plugged into the same USB port.
```

---

# Canonical USB Device Object

```json
{
  "device_id": "usb:05ac:110a:00000001",
  "device_stable_id": "audio.05ac.110a.serial-00000001",
  "persistence_mode": "serial_based",

  "vendor_id": "05ac",
  "product_id": "110a",

  "manufacturer": "Apple",
  "product_name": "USB-C Audio Adapter",
  "serial_number": "00000001",

  "usb_port_path": "1-1.2",
  "devpath": "/devices/platform/.../usb1/1-1/1-1.2",

  "device_types": ["audio_dac"],

  "connected": true,

  "first_seen": "2026-06-09T10:00:00Z",
  "last_seen": "2026-06-09T12:00:00Z",
  "created_at": "2026-06-09T10:00:00Z",
  "updated_at": "2026-06-09T12:00:00Z"
}
```

---

# Audio DAC Detection

A USB device should be classified as `audio_dac` when it exposes an ALSA sound card or has USB audio interface metadata.

Useful indicators:

```text
ID_USB_INTERFACES contains audio interface
ID_USB_DRIVER=snd-usb-audio
subsystem=sound
ALSA card exists under /sys/class/sound
```

---

# Audio Capability Object

```json
{
  "device_stable_id": "audio.05ac.110a.serial-00000001",

  "alsa_card": 2,
  "alsa_device": 0,
  "alsa_hw": "hw:2,0",

  "alsa_card_id": "Device",
  "alsa_card_name": "USB Audio",
  "alsa_long_name": "USB Audio at usb-0000:01:00.0-1.2, full speed",

  "formats": ["S16_LE", "S24_3LE"],
  "sample_rates": [44100, 48000, 96000],

  "max_sample_rate": 96000,
  "max_bit_depth": 24,

  "created_at": "2026-06-09T10:00:00Z",
  "updated_at": "2026-06-09T12:00:00Z"
}
```

---

# ALSA Persistence Strategy

ALSA card numbers are not persistent.

Do not store:

```text
hw:2,0
plughw:2,0
card 2
```

as user selections.

Instead, store:

```text
DEVICE_STABLE_ID
```

Then create persistent HomePi ALSA mappings for selected audio devices.

---

# User-Selected USB Assignments

The user may select USB devices for these roles:

```text
hifi_serial
primary_audio
paging_audio
```

These assignments must be stored by `DEVICE_STABLE_ID`.

Example:

```json
{
  "assignment_key": "primary_audio",
  "device_stable_id": "audio.05ac.110a.serial-00000001"
}
```

---

# Assignment Rules

## HiFi Serial Assignment

When the user selects a HiFi serial USB adapter:

```text
assignment_key = hifi_serial
device_type must include serial
```

Create persistent symlink:

```text
/dev/homepi/hifi-serial
```

The HiFi controller must use:

```text
/dev/homepi/hifi-serial
```

not:

```text
/dev/ttyUSB0
```

---

## Primary Audio Assignment

When the user selects the primary audio DAC:

```text
assignment_key = primary_audio
device_type must include audio_dac
```

Create stable ALSA card ID:

```text
HomePiPrimary
```

Create ALSA PCM alias:

```text
homepi_primary
```

Audio services must use:

```text
homepi_primary
```

not:

```text
hw:2,0
```

---

## Paging Audio Assignment

When the user selects the paging audio DAC:

```text
assignment_key = paging_audio
device_type must include audio_dac
```

Create stable ALSA card ID:

```text
HomePiPaging
```

Create ALSA PCM alias:

```text
homepi_paging
```

Paging services must use:

```text
homepi_paging
```

not:

```text
hw:3,0
```

---

# udev Rule Generation

Only create udev rules for user-selected assignments.

Do not create udev rules for every detected device.

Rule:

```text
detected device -> inventory only
user-selected device -> inventory + assignment + udev rule
```

---

# udev Rule File

Generated file:

```text
/etc/udev/rules.d/90-homepi-usb.rules
```

This file must be fully managed by HomePi.

It should include a clear warning header:

```text
# This file is managed by HomePi.
# Do not edit manually.
# Changes may be overwritten by homepi-usb-devices.
```

---

# Serial udev Rule Example

For a serial adapter with a real serial number:

```udev
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", ATTRS{serial}=="A50285BI", SYMLINK+="homepi/hifi-serial", GROUP="dialout", MODE="0660"
```

If `/dev/serial/by-id` is available, still create `/dev/homepi/hifi-serial` for HomePi consistency.

---

# Audio udev Rule Example: Serial-Based

For a selected primary DAC with a unique serial number:

```udev
SUBSYSTEM=="sound", KERNEL=="card*", ATTRS{idVendor}=="05ac", ATTRS{idProduct}=="110a", ATTRS{serial}=="00000001", ATTR{id}="HomePiPrimary"
```

---

# Audio udev Rule Example: Port-Based

For a selected primary DAC without a unique serial number:

```udev
SUBSYSTEM=="sound", KERNEL=="card*", ATTRS{idVendor}=="1234", ATTRS{idProduct}=="5678", KERNELS=="1-1.2", ATTR{id}="HomePiPrimary"
```

For paging:

```udev
SUBSYSTEM=="sound", KERNEL=="card*", ATTRS{idVendor}=="1234", ATTRS{idProduct}=="5678", KERNELS=="1-1.3", ATTR{id}="HomePiPaging"
```

---

# ALSA Alias Generation

Generated file:

```text
/etc/alsa/conf.d/90-homepi.conf
```

Preferred aliases:

```conf
pcm.homepi_primary {
    type plug
    slave.pcm "hw:CARD=HomePiPrimary,DEV=0"
}

pcm.homepi_paging {
    type plug
    slave.pcm "hw:CARD=HomePiPaging,DEV=0"
}
```

Services should reference:

```text
homepi_primary
homepi_paging
```

---

# Reload Behavior

After writing udev rules:

```text
udev rules must be reloaded
udev trigger may be required
affected HomePi services may need restart
```

The service should not reboot the Raspberry Pi automatically.

Expected commands, implemented internally through a controlled privileged helper or install-managed script:

```bash
udevadm control --reload-rules
udevadm trigger
```

After remapping audio devices, services that already opened ALSA devices may need to restart.

Examples:

```text
homepi-pcm-router
homepi-paging
shairport-sync zone services
```

---

# Storage Tables

Storage is owned by `/core/storage`.

These schemas describe the required data model only.

---

## CORE_USB_DEVICES

```sql
CREATE TABLE CORE_USB_DEVICES
(
    DEVICE_ID TEXT PRIMARY KEY,
    DEVICE_STABLE_ID TEXT NOT NULL UNIQUE,

    PERSISTENCE_MODE TEXT NOT NULL,

    VENDOR_ID TEXT,
    PRODUCT_ID TEXT,

    MANUFACTURER TEXT,
    PRODUCT_NAME TEXT,
    SERIAL_NUMBER TEXT,

    USB_PORT_PATH TEXT,
    DEVPATH TEXT,

    DEVICE_TYPES_JSON TEXT NOT NULL,

    CONNECTED INTEGER NOT NULL DEFAULT 0,

    FIRST_SEEN TEXT,
    LAST_SEEN TEXT,

    CREATED_AT TEXT,
    UPDATED_AT TEXT
);
```

---

## CORE_USB_AUDIO_CAPABILITIES

```sql
CREATE TABLE CORE_USB_AUDIO_CAPABILITIES
(
    DEVICE_STABLE_ID TEXT PRIMARY KEY,

    ALSA_CARD INTEGER,
    ALSA_DEVICE INTEGER,
    ALSA_HW TEXT,

    ALSA_CARD_ID TEXT,
    ALSA_CARD_NAME TEXT,
    ALSA_LONG_NAME TEXT,

    FORMATS_JSON TEXT,
    SAMPLE_RATES_JSON TEXT,

    MAX_SAMPLE_RATE INTEGER,
    MAX_BIT_DEPTH INTEGER,

    CREATED_AT TEXT,
    UPDATED_AT TEXT
);
```

---

## CORE_USB_SERIAL_CAPABILITIES

```sql
CREATE TABLE CORE_USB_SERIAL_CAPABILITIES
(
    DEVICE_STABLE_ID TEXT PRIMARY KEY,

    TTY_DEVICE TEXT,
    BY_ID_PATH TEXT,
    DRIVER_NAME TEXT,

    CREATED_AT TEXT,
    UPDATED_AT TEXT
);
```

---

## CORE_USB_ASSIGNMENTS

```sql
CREATE TABLE CORE_USB_ASSIGNMENTS
(
    ASSIGNMENT_KEY TEXT PRIMARY KEY,

    DEVICE_STABLE_ID TEXT NOT NULL,

    ASSIGNMENT_LABEL TEXT,

    CREATED_AT TEXT,
    UPDATED_AT TEXT
);
```

Allowed assignment keys:

```text
hifi_serial
primary_audio
paging_audio
```

---

# Service Startup Flow

## Step 1: Initialize Core Services

```text
Initialize core/logging
Initialize core/storage client
Load USB configuration
```

---

## Step 2: Load Assignments

Read:

```text
CORE_USB_ASSIGNMENTS
```

into memory.

---

## Step 3: Build Startup Snapshot

Scan currently connected USB devices using `libudev`.

For each device:

```text
read vendor/product/serial/manufacturer/product
detect USB port path
classify device type
generate DEVICE_STABLE_ID
probe capabilities
persist inventory through core/storage
```

---

## Step 4: Validate Assignments

For each stored assignment:

```text
check if selected device is connected
check if selected device still has required type
check if udev rule exists
check if ALSA alias exists for audio assignments
```

If missing, regenerate rules.

---

## Step 5: Start udev Monitor

Listen for:

```text
add
remove
change
```

---

# Runtime Event Flow

## Device Connected

```text
udev add event
read device metadata
classify device
generate stable ID
probe capabilities
persist connected=true
publish usb.device.connected
if device matches assignment, validate mapping
```

---

## Device Disconnected

```text
udev remove event
resolve known stable ID
persist connected=false
publish usb.device.disconnected
do not delete device record
do not delete assignment
```

---

## Device Changed

```text
udev change event
refresh metadata
refresh ALSA or serial capability
persist updates
publish usb.device.updated
```

---

# API

The USB module should expose internal APIs for other HomePi services.

Transport can be decided later.

Acceptable transports:

```text
Unix domain socket
core/events request-response
local HTTP bound to localhost only
```

---

## List Devices

Request:

```json
{
  "action": "list_devices"
}
```

Response:

```json
{
  "devices": []
}
```

---

## List Audio Devices

Request:

```json
{
  "action": "list_audio_devices"
}
```

---

## List Serial Devices

Request:

```json
{
  "action": "list_serial_devices"
}
```

---

## Get Device

Request:

```json
{
  "action": "get_device",
  "device_stable_id": "audio.05ac.110a.serial-00000001"
}
```

---

## Assign Device

Request:

```json
{
  "action": "assign_device",
  "assignment_key": "primary_audio",
  "device_stable_id": "audio.05ac.110a.serial-00000001"
}
```

Validation:

```text
assignment_key must be valid
device must exist
device must be connected
device type must match assignment
```

After assignment:

```text
persist assignment
generate udev rule
generate ALSA alias if audio
reload udev rules
publish usb.assignment.updated
```

---

## Unassign Device

Request:

```json
{
  "action": "unassign_device",
  "assignment_key": "primary_audio"
}
```

After unassignment:

```text
remove assignment
regenerate HomePi udev rule file
regenerate ALSA alias file
reload udev rules
publish usb.assignment.removed
```

---

# Events

Events should be published through the HomePi event mechanism when available.

---

## usb.snapshot.completed

```json
{
  "event": "usb.snapshot.completed",
  "connected_count": 4,
  "audio_count": 2,
  "serial_count": 1
}
```

---

## usb.device.connected

```json
{
  "event": "usb.device.connected",
  "device_stable_id": "serial.ftdi.a50285bi",
  "device_types": ["serial"]
}
```

---

## usb.device.disconnected

```json
{
  "event": "usb.device.disconnected",
  "device_stable_id": "serial.ftdi.a50285bi"
}
```

---

## usb.device.updated

```json
{
  "event": "usb.device.updated",
  "device_stable_id": "audio.05ac.110a.serial-00000001"
}
```

---

## usb.assignment.updated

```json
{
  "event": "usb.assignment.updated",
  "assignment_key": "primary_audio",
  "device_stable_id": "audio.05ac.110a.serial-00000001"
}
```

---

# UI Requirements

The UI should show all connected USB devices.

For each device, show:

```text
Device name
Device type
Vendor ID
Product ID
Serial number
Persistence mode
Connection status
Current ALSA device if audio
Current tty path if serial
Assignment status
```

For audio devices:

```text
Sample rates
Formats
Max sample rate
Max bit depth
```

For serial devices:

```text
Current tty device
By-id path
Driver
```

---

# UI Assignment Options

User can assign:

```text
HiFi Serial USB
Primary Audio USB
Paging Audio USB
```

Assignment rules:

```text
HiFi Serial USB requires serial device
Primary Audio USB requires audio_dac
Paging Audio USB requires audio_dac
```

---

# Duplicate DAC Warning

If the selected DAC is port-based, display:

```text
This DAC does not expose a unique serial number.
HomePi will persist this assignment by USB port.
Keep this DAC plugged into the same USB port.
```

---

# Example Complete Inventory Response

```json
{
  "devices": [
    {
      "device_id": "usb:0403:6001:A50285BI",
      "device_stable_id": "serial.ftdi.a50285bi",
      "persistence_mode": "by_id_based",
      "vendor_id": "0403",
      "product_id": "6001",
      "manufacturer": "FTDI",
      "product_name": "FT232R USB UART",
      "serial_number": "A50285BI",
      "usb_port_path": "1-1.1",
      "device_types": ["serial"],
      "connected": true,
      "serial": {
        "tty_device": "/dev/ttyUSB0",
        "by_id_path": "/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A50285BI-if00-port0",
        "driver_name": "ftdi_sio"
      },
      "assignment": {
        "assignment_key": "hifi_serial",
        "mapped_path": "/dev/homepi/hifi-serial"
      }
    },
    {
      "device_id": "usb:05ac:110a:00000001",
      "device_stable_id": "audio.05ac.110a.serial-00000001",
      "persistence_mode": "serial_based",
      "vendor_id": "05ac",
      "product_id": "110a",
      "manufacturer": "Apple",
      "product_name": "USB-C Audio Adapter",
      "serial_number": "00000001",
      "usb_port_path": "1-1.2",
      "device_types": ["audio_dac"],
      "connected": true,
      "audio": {
        "alsa_card": 2,
        "alsa_device": 0,
        "alsa_hw": "hw:2,0",
        "alsa_card_id": "HomePiPrimary",
        "alsa_card_name": "USB Audio",
        "formats": ["S16_LE", "S24_3LE"],
        "sample_rates": [44100, 48000, 96000],
        "max_sample_rate": 96000,
        "max_bit_depth": 24
      },
      "assignment": {
        "assignment_key": "primary_audio",
        "alsa_alias": "homepi_primary",
        "alsa_target": "hw:CARD=HomePiPrimary,DEV=0"
      }
    }
  ]
}
```

---

# systemd Service

File:

```text
/core/usb/systemd/homepi-usb-devices.service
```

Example:

```ini
[Unit]
Description=HomePi USB Devices Core Service
After=local-fs.target
Wants=local-fs.target

[Service]
Type=simple
ExecStart=/usr/local/bin/homepi-usb-devices
Restart=always
RestartSec=2
User=root
Group=root

NoNewPrivileges=false

[Install]
WantedBy=multi-user.target
```

Root is required because this service manages:

```text
/etc/udev/rules.d/90-homepi-usb.rules
/etc/alsa/conf.d/90-homepi.conf
udev reloads
```

If possible later, split privileged actions into a smaller helper.

---

# Error Handling

The service must log and continue on non-fatal errors.

Examples:

```text
ALSA probe failed for one device
udev event missing metadata
device disconnected during probe
udev reload failed
storage write failed
assignment references missing device
```

Fatal errors:

```text
core/logging unavailable
core/storage unavailable
udev monitor cannot initialize
```

---

# Performance Requirements

The service should be lightweight.

Expected behavior:

```text
No polling loop
No repeated shell commands
No blocking long-running probes on main udev monitor thread
Debounce noisy udev events
Cache known device metadata
Write storage only when values change
```

---

# Debounce Requirement

udev may emit multiple events for one physical action.

Implement debounce by device path.

Suggested debounce window:

```text
250ms to 500ms
```

---

# Security Requirements

Only generated files should be written:

```text
/etc/udev/rules.d/90-homepi-usb.rules
/etc/alsa/conf.d/90-homepi.conf
```

Validate all values written into udev rules.

Reject or escape:

```text
quotes
newlines
shell metacharacters
unexpected whitespace
```

Never include raw untrusted USB strings in executable commands.

---

# Acceptance Criteria

The module is complete when:

```text
Service starts on Raspberry Pi
Startup snapshot detects connected USB devices
USB connect event is detected
USB disconnect event is detected
Serial adapter is classified correctly
Audio DAC is classified correctly
Audio DAC sample rates are detected
Audio DAC formats are detected
DEVICE_STABLE_ID is generated for every device
Duplicate/no-serial DACs use port-based identity
Device inventory persists through core/storage
All logs use core/logging
User can assign hifi_serial
User can assign primary_audio
User can assign paging_audio
udev rule is created only for assigned devices
/dev/homepi/hifi-serial works for selected serial adapter
homepi_primary ALSA alias works for selected primary DAC
homepi_paging ALSA alias works for selected paging DAC
Reboot preserves user selections
Changing /dev/ttyUSB0 number does not break HiFi serial mapping
Changing ALSA card number does not break audio assignment
Port-based duplicate DAC warning is available to UI
```
