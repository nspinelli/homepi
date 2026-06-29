# HomePi Architecture Update for Cursor

**Document Type:** Cursor implementation architecture  
**Project:** HomePi  
**Target Repo:** `https://github.com/nspinelli/homepi`  
**Primary Goal:** An unrelated service failure must not disrupt other HomePi modules.  
**Status:** Draft for implementation review  

---

# 1. Purpose

This document defines the next HomePi architecture direction for Cursor-assisted development.

HomePi should be redesigned around isolated modules, dedicated Unix domain sockets, optional event fanout, meaningful service health, and clear failure boundaries.

The main point:

> An unrelated service failure must not disrupt other HomePi modules.

Examples:

```text
A PCM router failure must not stop contact sensors.
A HomeKit bridge failure must not stop sensor monitoring.
A broker failure must not prevent direct audio commands.
A paging failure must not take down normal audio zone control.
A database failure should not crash hardware-control services that can safely continue from memory.
A service that is active in systemd but functionally unusable must report a meaningful degraded status.
```

The architecture should be event-driven where appropriate, but it must not depend on one central event service for all commands and module survival.

---

# 2. High-Level Architecture Decision

HomePi should move from:

```text
central core/events runtime dependency
```

to:

```text
dedicated module sockets
module facade services
optional broker event fanout
systemd/D-Bus health observation
structured logs
schema-driven messages
```

The new communication rule:

```text
Commands are direct.
Events are fanout.
Health is observational.
Logs are diagnostic.
```

The broker should improve realtime updates, but it should never be required for direct commands.

---

# 3. Runtime Communication Model

## 3.1 Direct command path

Commands should flow through dedicated Unix sockets.

```text
UI
  -> homepi-backend / homepi-api
    -> module facade socket
      -> internal service socket when needed
```

Example:

```text
UI
  -> backend
    -> /run/homepi/audio/audio.sock
      -> /run/homepi/audio/hifi.sock
```

A zone power command should not require the broker.

This must continue working even if the broker is offline:

```text
UI -> backend -> audio.sock -> hifi.sock
```

## 3.2 Event fanout path

Events should be normalized at the module facade boundary and then published to the broker.

```text
internal service
  -> module facade
    -> broker
      -> backend/UI/HomeKit/health subscribers
```

Example:

```text
homepi-hifi
  -> homepi-audio
    -> homepi-broker
      -> backend WebSocket/SSE
```

The broker publishes state changes. It does not own the state machine.

## 3.3 Health path

Health should be monitored through systemd and D-Bus, plus service snapshots.

```text
systemd
  -> D-Bus
    -> homepi-health
      -> backend/UI
```

Services may also expose `getHealth` or `snapshot` commands on their own command sockets.

---

# 4. Required Architectural Rule

Every runtime service must be able to fail independently wherever possible.

A module or service may become degraded, but it must not cascade unrelated failures.

Expected behavior examples:

```text
homepi-pcm-router fails:
  Audio playback routing is degraded.
  Hi-Fi2 zone control can still work.
  Contact sensors continue.
  HomeKit continues unless it directly depends on the failed capability.

homepi-homekit fails:
  HomeKit bridge is offline.
  Contact sensors continue in HomePi.
  Audio continues.

homepi-broker fails:
  Live updates degrade.
  Direct commands continue.
  Module snapshots continue.

homepi-database fails:
  Runtime control continues where safe.
  Settings writes fail with a meaningful error.
```

---

# 5. Remove `core/events` as a Required Runtime Service

The current `core/events` concept should be reworked.

## 5.1 Remove

```text
core/events as the required central runtime bus for commands and service survival
```

## 5.2 Replace with

```text
core/messaging as shared schemas and helpers
homepi-broker as optional event fanout
dedicated sockets for direct service commands
homepi-health for systemd/D-Bus service monitoring
```

Recommended structure:

```text
core/
  messaging/
    envelope.ts
    request.ts
    response.ts
    event.ts
    errors.ts
    socket-client.ts
    socket-server.ts
    correlation.ts
    schemas/

services/
  broker/
    homepi-broker

services/
  health/
    homepi-health
```

The event concept remains.

The central required event-service dependency does not.

---

# 6. `core/messaging`

`core/messaging` is a shared library, not a runtime service.

It owns reusable message and socket mechanics.

Responsibilities:

```text
message envelopes
request/response schema
event schema
error schema
correlation ID helpers
socket server helpers
socket client helpers
timeout helpers
retry/backoff helpers
message size limits
schema validation helpers
structured error normalization
```

It must not own:

```text
audio policy
sensor policy
HomeKit policy
Hi-Fi2 protocol
PCM routing
USB device identity
database persistence
service health decisions
```

Correct dependency direction:

```text
services use core/messaging
core/messaging knows nothing about service domains
```

---

# 7. `homepi-broker`

`homepi-broker` is an optional event fanout service.

Runtime socket:

```text
/run/homepi/broker/broker.sock
```

Allowed operations:

```text
publish
subscribe
unsubscribe
snapshot
ping
```

Not allowed:

```text
routeCommand
callService
controlHardware
ownModuleState
ownServiceHealth
ownAudioStateMachine
```

If the broker fails:

```text
direct commands continue
modules continue running
live UI updates may degrade
snapshots still work through module sockets
health reports broker as degraded/offline
```

User-facing message:

```text
Live updates are temporarily unavailable, but direct HomePi control is still working.
```

---

# 8. Dedicated Socket Layout

Recommended runtime socket layout:

```text
/run/homepi/
  broker/
    broker.sock

  health/
    health.sock

  audio/
    audio.sock
    hifi.sock
    hifi-serial.sock
    pcm-router.sock
    airplay.sock
    paging.sock
    metadata.sock
    audio-realtime.sock

  sensors/
    sensors.sock

  homekit/
    homekit.sock

  usb/
    usb.sock
```

Rules:

```text
<module>/<module>.sock is the public module facade socket.
<module>/<internal-service>.sock is an internal service command socket.
broker/broker.sock is optional event fanout.
health/health.sock is normalized system/module health.
```

Optional event sockets may exist for internal service streams, but they should not replace direct command sockets.

---

# 9. Socket Permissions

Production sockets must not be world-writable.

Default:

```text
owner: service-specific user
group: homepi
mode: 0660
```

Example:

```text
/run/homepi/audio/audio.sock
  owner: homepi-audio
  group: homepi
  mode: 0660
```

Only trusted services should belong to the `homepi` group.

Examples:

```text
homepi-backend
homepi-health
homepi-audio
homepi-homekit
```

---

# 10. Command Message Contract

All command sockets should use the same request/response envelope.

Transport:

```text
Unix domain socket
NDJSON
one JSON object per line
strict schema validation
bounded message size
correlation IDs
timeouts
structured errors
```

Example request:

```json
{
  "v": 1,
  "id": "req_01J...",
  "source": "homepi-backend",
  "target": "homepi-audio",
  "command": "audio.zone.setPower",
  "correlationId": "ui:audio:zone-1:power:req_01J...",
  "payload": {
    "zone": 1,
    "power": true
  }
}
```

Example success:

```json
{
  "v": 1,
  "id": "req_01J...",
  "ok": true,
  "result": {
    "zone": 1,
    "power": true,
    "source": 7
  }
}
```

Example failure:

```json
{
  "v": 1,
  "id": "req_01J...",
  "ok": false,
  "error": {
    "code": "HIFI_CONTROLLER_UNAVAILABLE",
    "severity": "error",
    "userMessage": "Home audio is offline because the Hi-Fi2 controller is not responding.",
    "developerMessage": "homepi-hifi did not receive a valid controller response before timeout.",
    "service": "homepi-hifi",
    "recoverable": true,
    "retryable": true,
    "details": {
      "command": "audio.zone.setPower",
      "zone": 1
    }
  }
}
```

---

# 11. Event Contract

Broker events should be normalized app-level events.

Topic format:

```text
homepi.<module>.<entity>.<event>
```

Examples:

```text
homepi.audio.zone.changed
homepi.audio.capability.changed
homepi.audio.paging.started
homepi.audio.paging.failed
homepi.audio.airplay.receiver.changed

homepi.sensors.contact.changed
homepi.sensors.tamper.changed
homepi.sensors.fault.changed

homepi.usb.device.connected
homepi.usb.device.disconnected
homepi.usb.mapping.changed

homepi.homekit.bridge.ready
homepi.homekit.bridge.failed

homepi.health.service.changed
```

Example event:

```json
{
  "v": 1,
  "id": "evt_01J...",
  "ts": "2026-06-28T14:00:00.000Z",
  "topic": "homepi.audio.zone.changed",
  "source": "homepi-audio",
  "correlationId": "req_01J...",
  "severity": "info",
  "payload": {
    "zone": 1,
    "power": true,
    "source": 7,
    "volume": 35
  }
}
```

Internal service details should not flood the global broker.

Examples of internal details that should remain internal unless normalized by the facade:

```text
homepi-hifi raw protocol lines
homepi-pcm-router buffer states
homepi-metadata parser internals
ALSA period callbacks
serial byte reads
```

---

# 12. Module Facade Pattern

Each user-facing module should have one facade service.

Examples:

```text
homepi-audio
homepi-sensors
homepi-homekit
homepi-usb
homepi-health
```

A module facade owns the public API for that module.

The backend should prefer calling the facade rather than many internal service sockets.

Preferred:

```text
backend
  -> /run/homepi/audio/audio.sock
```

Avoid:

```text
backend
  -> /run/homepi/audio/hifi.sock
  -> /run/homepi/audio/pcm-router.sock
  -> /run/homepi/audio/paging.sock
  -> /run/homepi/audio/airplay.sock
```

Internal services can still expose sockets, but the facade coordinates most module-level behavior.

---

# 13. Audio Module Architecture

Audio should be treated as one user-facing module with multiple backend services.

User-facing module:

```text
homepi-audio
```

Internal services:

```text
homepi-hifi
homepi-hifi-serial
homepi-pcm-router
homepi-airplay-manager
homepi-audio-paging
homepi-metadata
homepi-nqptp
homepi-shairport-supervisor
homepi-shairport@N
```

## 13.1 `homepi-audio`

Owns:

```text
audio module facade API
zone model
source model
desired state
capability status
coordination between internal audio services
normalized audio events
user-facing degraded messages
```

Does not own:

```text
raw serial port
raw Hi-Fi2 command formatting
ALSA PCM routing
Shairport process execution
TTS engine internals
USB device identity
metadata parsing internals
```

## 13.2 `homepi-hifi`

Owns:

```text
Hi-Fi2 domain commands
zone/source/group state
typed command API
controller state reconciliation
confirmed controller state
Hi-Fi2 command queue
50 ms command spacing
controller protocol parsing
```

Only this service should understand Hi-Fi2 command behavior at the protocol level.

No other service should format raw protocol commands such as:

```text
*Z1POWER1
*Z1SRC7
*Z1VOLUME35
*PAGE1
```

Other services must send typed commands.

## 13.3 `homepi-hifi-serial`

Owns:

```text
physical serial adapter availability
serial device validation
controller handshake
serial connection status
low-level serial read/write path if separated from homepi-hifi
```

If the selected USB serial device is missing, this service becomes degraded/offline.

The rest of HomePi must keep running.

## 13.4 `homepi-pcm-router`

Owns:

```text
ALSA loopback capture
DAC playback
active PCM route
owner selection
stream readiness
audio data plane
```

Does not own:

```text
Hi-Fi2 zone power
Hi-Fi2 source selection
AirPlay process lifecycle
UI zone names
HomeKit state
```

A PCM router failure should degrade playback routing only.

## 13.5 `homepi-airplay-manager`

Owns:

```text
Shairport Sync service lifecycle
per-zone AirPlay receiver availability
generated Shairport config
receiver enable/disable
receiver restart actions
```

This service should replace direct backend/systemctl knowledge of every Shairport zone where possible.

## 13.6 `homepi-audio-paging`

Owns:

```text
TTS request lifecycle
voice preview
page start
page playback
page stop
paging DAC readiness
paging-specific errors
```

A paging failure must not take down normal audio zone control.

## 13.7 `homepi-metadata`

Owns:

```text
Shairport metadata input
now-playing reducer
cover art cache
play history
audio realtime socket
latest-value playback telemetry
```

High-frequency playback telemetry must not go through the broker.

---

# 14. Audio Capability Model

The audio module should report capability-level health.

Example:

```json
{
  "module": "audio",
  "overall": "degraded",
  "capabilities": {
    "zoneControl": {
      "available": true,
      "provider": "homepi-hifi"
    },
    "airplayReceivers": {
      "available": true,
      "provider": "homepi-airplay-manager"
    },
    "pcmRouting": {
      "available": false,
      "provider": "homepi-pcm-router",
      "userMessage": "Playback routing is offline because the selected primary audio DAC is unavailable."
    },
    "metadata": {
      "available": true,
      "provider": "homepi-metadata"
    },
    "paging": {
      "available": false,
      "provider": "homepi-audio-paging",
      "userMessage": "Paging is unavailable because the selected paging DAC is not connected."
    }
  }
}
```

The UI should show:

```text
Audio: Degraded
  Zone Control: Online
  AirPlay: Online
  PCM Routing: Offline
  Metadata: Online
  Paging: Offline
```

not:

```text
Audio: Failed
```

---

# 15. Contact Sensors Module Architecture

The sensors module should remain event-driven.

Service:

```text
homepi-sensors
```

Socket:

```text
/run/homepi/sensors/sensors.sock
```

Owns:

```text
GPIO line configuration
MCP23017 configuration
interrupt handling
contact sensor state
tamper/fault state
sensor enable/disable state
room assignment data
HomeKit exposure eligibility
```

Does not own:

```text
HomeKit bridge runtime
frontend state store
global broker runtime
database internals
```

If HomeKit fails, sensors should continue to work in the HomePi UI.

If the broker fails, sensor snapshots should still be available through `sensors.sock`.

The sensors service should publish normalized events such as:

```text
homepi.sensors.contact.changed
homepi.sensors.tamper.changed
homepi.sensors.fault.changed
homepi.sensors.config.changed
```

---

# 16. HomeKit Module Architecture

Service:

```text
homepi-homekit
```

Socket:

```text
/run/homepi/homekit/homekit.sock
```

Owns:

```text
HAP-NodeJS bridge lifecycle
HomeKit accessory registration
HomeKit accessory state projection
HomeKit pairing state
HomeKit bridge status
```

Does not own:

```text
raw sensor GPIO
raw audio service internals
USB device identity
broker runtime
```

HomeKit should consume normalized module events and/or request snapshots from module facade sockets.

If HomeKit is offline:

```text
Sensors continue.
Audio continues.
Backend continues.
UI shows HomeKit bridge offline.
```

User-facing message:

```text
HomeKit bridge is offline, but HomePi modules are still running.
```

---

# 17. USB Module Architecture

Service:

```text
homepi-usb
```

Socket:

```text
/run/homepi/usb/usb.sock
```

Owns:

```text
USB device detection
stable device identity
role assignment
udev rule generation
selected device readiness
device connect/disconnect events
```

Example roles:

```text
Primary_Audio
Primary_Paging
HIFI_Serial
```

Does not own:

```text
PCM routing
Hi-Fi2 command protocol
paging playback
frontend layout
```

If USB service fails:

```text
Already-opened hardware may continue where safe.
New device mapping is unavailable.
Dependent capabilities become degraded only if they require a missing device.
```

---

# 18. Health Service Architecture

Service:

```text
homepi-health
```

Socket:

```text
/run/homepi/health/health.sock
```

Owns:

```text
systemd service observation
D-Bus lifecycle monitoring
service registry loading
socket availability checks
readiness snapshots
recent structured error summaries
normalized module health
```

Does not own:

```text
module commands
hardware control
broker command routing
domain state machines
```

The health service should distinguish:

```text
process status
readiness status
domain status
```

Example:

```json
{
  "service": "homepi-pcm-router",
  "process": "active",
  "readiness": "not_ready",
  "domain": "missing_device",
  "userMessage": "Playback routing is unavailable because the primary audio DAC is not connected."
}
```

This avoids misleading UI states where systemd says a process is active but the feature is not usable.

---

# 19. Service Registry

HomePi should have a static service registry.

Recommended location:

```text
config/services/
  audio.json
  sensors.json
  homekit.json
  usb.json
  core.json
```

or:

```text
core/service-registry/
  registry.schema.json
  services/*.json
```

Example:

```json
{
  "module": "audio",
  "services": [
    {
      "name": "homepi-audio",
      "unit": "homepi-audio.service",
      "role": "module-facade",
      "commandSocket": "/run/homepi/audio/audio.sock",
      "critical": true
    },
    {
      "name": "homepi-hifi",
      "unit": "homepi-hifi.service",
      "role": "hardware-controller",
      "commandSocket": "/run/homepi/audio/hifi.sock",
      "critical": true
    },
    {
      "name": "homepi-pcm-router",
      "unit": "homepi-pcm-router.service",
      "role": "data-plane",
      "commandSocket": "/run/homepi/audio/pcm-router.sock",
      "critical": false
    }
  ]
}
```

The registry should tell `homepi-health`:

```text
which units exist
which sockets should exist
which module owns each service
which services are critical
which failures affect which capabilities
which user-facing message category applies
```

---

# 20. systemd Dependency Rules

Avoid hard chains between unrelated services.

Bad:

```ini
Requires=homepi-pcm-router.service
Requires=homepi-homekit.service
Requires=homepi-broker.service
```

Better:

```ini
Wants=homepi-pcm-router.service
Wants=homepi-homekit.service
Wants=homepi-broker.service
After=network-online.target
```

Use hard requirements only when the service truly cannot start without that dependency.

For most HomePi services, the correct behavior is:

```text
Start.
Detect unavailable capabilities.
Report degraded state.
Keep unrelated functionality online.
```

Recommended unit behavior:

```ini
Restart=on-failure
RestartSec=2
RuntimeDirectory=homepi/<module>
RuntimeDirectoryMode=0750
WatchdogSec=30
```

Use `WatchdogSec` only for services that correctly notify systemd.

---

# 21. Database Failure Behavior

Database access should be centralized through the chosen storage/database layer.

Runtime services should not crash simply because persistence is temporarily unavailable.

If the database is unavailable:

```text
safe runtime hardware control may continue
settings writes should fail cleanly
last known in-memory state may be used where safe
health should show persistence degraded
logs should include developer details
UI should show a meaningful message
```

Example user-facing message:

```text
Settings cannot be saved right now because the database is unavailable, but runtime control is still active.
```

---

# 22. Logging Requirements

All services should use the shared HomePi logging contract.

Every log should include:

```text
timestamp
service
level
event
message
correlationId
structured data
```

Avoid:

```text
ad-hoc printf logs
module-specific logger formats
module-specific log files
unstructured stdout text
```

Repeated warnings/errors should be rate-limited.

Logs should support:

```text
backend diagnostics
health summaries
UI recent activity
post-failure debugging
AI-readable troubleshooting
```

---

# 23. Backend/API Role

The backend should act as the external API gateway.

It should:

```text
serve HTTP routes
serve frontend realtime channels
proxy commands to module facades
normalize socket errors
expose snapshots
expose health summaries
authenticate/authorize future requests if needed
```

It should not:

```text
own hardware
own audio routing policy
talk to every internal audio service unless required
replace module facades
infer service health from only HTTP failures
```

Preferred flow:

```text
Frontend
  -> backend
    -> module facade socket
      -> internal service socket
```

---

# 24. Frontend/UI Behavior

The UI should show module-level and capability-level status.

Examples:

```text
Audio: Degraded
  Zone Control: Online
  AirPlay: Online
  PCM Routing: Offline
  Paging: Offline

Sensors: Online
HomeKit: Offline
USB Devices: Online
Broker: Offline
```

The UI should never show a generic failure when a meaningful message is available.

Preferred message format:

```text
What is broken?
What still works?
What action can the user take?
```

Example:

```text
Playback routing is offline because the selected primary audio DAC is unavailable. Zone control and AirPlay receivers are still online.
```

---

# 25. Failure Isolation Examples

## 25.1 PCM router fails

Expected behavior:

```text
Audio module becomes degraded.
Zone control remains available.
AirPlay receivers may remain visible.
Paging may degrade if it depends on PCM output.
Sensors are unaffected.
HomeKit is unaffected.
```

User message:

```text
Playback routing is offline because the selected primary audio DAC is unavailable.
```

## 25.2 Hi-Fi2 controller unavailable

Expected behavior:

```text
Audio zone control is unavailable.
PCM router may still be running.
AirPlay receivers may still be visible.
Paging unavailable if it requires Hi-Fi2 PAGE commands.
Sensors are unaffected.
HomeKit is unaffected.
```

User message:

```text
Home audio control is offline because the Hi-Fi2 controller is not responding.
```

## 25.3 HomeKit bridge fails

Expected behavior:

```text
HomeKit module is offline.
Sensors continue in HomePi.
Sensor events continue.
Audio continues.
```

User message:

```text
HomeKit bridge is offline, but HomePi modules are still running.
```

## 25.4 Broker fails

Expected behavior:

```text
Direct commands continue.
Live event fanout stops or degrades.
Manual refresh and snapshots continue.
Health reports broker offline.
```

User message:

```text
Live updates are unavailable, but direct HomePi control is still working.
```

## 25.5 Database fails

Expected behavior:

```text
Runtime control continues where safe.
Configuration writes fail.
Health reports persistence degraded.
UI shows save-related errors only where applicable.
```

User message:

```text
Settings cannot be saved right now because HomePi storage is unavailable.
```

---

# 26. Proposed Repository Organization

Recommended target layout:

```text
core/
  messaging/
    envelope.ts
    request.ts
    response.ts
    event.ts
    errors.ts
    correlation.ts
    socket-client.ts
    socket-server.ts
    schemas/

  logging/
    native/
    node/
    schemas/

  database/
    client.ts
    migrations/
    schemas/

  service-registry/
    registry.ts
    registry.schema.json
    services/

apps/
  backend/
  frontend/

services/
  broker/
    homepi-broker

  health/
    homepi-health

  audio/
    homepi-audio
    homepi-hifi
    homepi-hifi-serial
    homepi-pcm-router
    homepi-airplay-manager
    homepi-audio-paging
    homepi-metadata

  sensors/
    homepi-sensors

  homekit/
    homepi-homekit

  usb/
    homepi-usb

modules/
  audio/
    schema/
    docs/
    ui-contracts/

  sensors/
    schema/
    docs/
    ui-contracts/

  homekit/
    schema/
    docs/
    ui-contracts/

infra/
  systemd/
  nginx/
  udev/

docs/
  architecture/
  services/
  runbooks/

tooling/
  build/
  install/
  verify/
```

---

# 27. Migration Plan

## Phase 1: Document and freeze contracts

Create or update:

```text
docs/architecture/homepi-service-isolation.md
docs/architecture/sockets-and-broker.md
docs/architecture/service-health.md
docs/architecture/audio-module.md
```

Define:

```text
message envelope
error envelope
event envelope
socket naming
service registry schema
health snapshot schema
```

## Phase 2: Introduce `core/messaging`

Move reusable transport/message logic into:

```text
core/messaging
```

Do not make it a runtime service.

## Phase 3: Add `homepi-broker`

Create broker as optional event fanout.

Do not route commands through the broker.

## Phase 4: Add `homepi-health`

Implement health service using:

```text
systemd/D-Bus
service registry
socket availability
service snapshots
structured logs
```

## Phase 5: Refactor audio around facade pattern

Create or formalize:

```text
homepi-audio
```

as the audio module facade.

Audio backend should route commands through:

```text
/run/homepi/audio/audio.sock
```

Internal audio service sockets remain private to the audio module.

## Phase 6: Refactor current `core/events` usage

Replace command-routing behavior with direct sockets.

Keep normalized events through broker.

## Phase 7: Update backend bridge

Backend should:

```text
call module facade sockets
subscribe to broker for live updates
read health snapshots from homepi-health
fallback to direct snapshots when broker is offline
```

## Phase 8: Update systemd units

Change dependency behavior from hard `Requires` to safer `Wants` where appropriate.

Each unit should support:

```text
Restart=on-failure
WatchdogSec where appropriate
structured journald logs
clear service user/group
runtime directory ownership
```

## Phase 9: Update frontend status model

Frontend should show:

```text
module status
capability status
service detail
meaningful user messages
last updated
available actions
```

---

# 28. Acceptance Criteria

The architecture is successful when the following are true:

```text
1. Stopping the broker does not prevent direct audio commands.

2. Stopping HomeKit does not stop sensor detection.

3. Stopping PCM router does not stop Hi-Fi2 zone control.

4. Removing the paging DAC only degrades paging.

5. Removing the primary audio DAC degrades playback routing but not sensors or HomeKit.

6. Restarting homepi-backend does not require native services to restart.

7. Restarting a native service does not require the entire app to restart.

8. Health distinguishes process state from feature readiness.

9. UI shows meaningful degraded messages.

10. Every service has a documented socket, owner, schema, and failure boundary.

11. Commands have correlation IDs.

12. Logs are structured and traceable across services.

13. No raw PCM, album art blobs, or high-frequency progress ticks go through the broker.

14. No unrelated service can block another module’s direct command path.
```

---

# 29. Cursor Rules

These rules must be followed by Cursor when modifying the HomePi repository.

## 29.1 No assumptions

Do not guess service behavior, file paths, socket names, schemas, GPIO pins, ALSA devices, USB roles, or systemd unit behavior.

If a required detail is missing, Cursor must either:

```text
ask for clarification
or create a clearly marked TODO
or add a documented configuration field
```

Cursor must not silently invent runtime behavior.

## 29.2 Contracts first

Before implementing service behavior, define or update the relevant contract.

Required contracts include:

```text
socket request schema
socket response schema
event schema
error schema
health snapshot schema
service registry entry
systemd unit expectations
logging event names
```

No service should expose undocumented commands.

## 29.3 Dedicated sockets for commands

Cursor must not route service commands through the broker.

Correct:

```text
backend -> module facade socket -> internal service socket
```

Incorrect:

```text
backend -> broker -> service command
```

The broker is for event fanout only.

## 29.4 Broker is optional

Any implementation that requires `homepi-broker` to be online for direct module commands is incorrect.

If the broker is offline:

```text
commands must still work
snapshots must still work
module services must continue running
health must report broker degraded/offline
```

## 29.5 Module isolation

Cursor must preserve module isolation.

Do not create dependencies where:

```text
audio failure stops sensors
HomeKit failure stops sensors
PCM router failure stops Hi-Fi2 zone control
broker failure stops direct commands
database failure crashes hardware services that can safely continue
```

## 29.6 Hardware ownership

Each hardware path must have exactly one owning service.

Examples:

```text
homepi-hifi or homepi-hifi-serial owns Hi-Fi2 serial/TCP control.
homepi-pcm-router owns ALSA PCM routing.
homepi-usb owns USB device identity and role assignment.
homepi-sensors owns GPIO/MCP23017 inputs.
homepi-homekit owns HAP-NodeJS bridge state.
homepi-audio-paging owns TTS paging lifecycle.
```

Do not allow multiple services to write directly to the same hardware interface.

## 29.7 Audio control plane vs data plane

Cursor must keep audio control messages separate from raw audio data.

Allowed over command sockets:

```text
zone power
zone volume
source selection
route selection
paging command
health snapshot
capability status
```

Not allowed over command sockets or broker:

```text
PCM frames
album art binary blobs
high-frequency playback ticks
raw metadata XML spam
serial byte streams
```

## 29.8 PCM router boundaries

`homepi-pcm-router` must not control Hi-Fi2 zones.

It owns:

```text
ALSA loopback capture
DAC playback
active PCM routing
stream readiness
```

It must not own:

```text
zone power
zone source
HomeKit
AirPlay service lifecycle
UI zone model
```

## 29.9 Hi-Fi2 boundaries

Only the Hi-Fi service should format raw Hi-Fi2 protocol commands.

Other services must call typed commands such as:

```text
hifi.zone.setPower
hifi.zone.setSource
hifi.zone.setVolume
hifi.page.setState
```

Do not scatter raw protocol strings like `*Z1POWER1` across the codebase.

## 29.10 Health is layered

Cursor must model health with separate layers:

```text
process status
readiness status
domain status
```

Do not treat `systemd active` as equivalent to feature availability.

A service can be:

```text
process: active
readiness: not_ready
domain: missing_device
```

## 29.11 Meaningful errors

Every service error returned to the backend must include:

```text
code
severity
userMessage
developerMessage
service
recoverable
retryable
details
correlationId when available
```

The UI should use `userMessage`.

Logs should include `developerMessage` and `details`.

## 29.12 Structured logging only

Cursor must use HomePi structured logging.

Every log should include:

```text
timestamp
service
level
event
message
correlationId
data
```

Avoid:

```text
printf debugging
unstructured stdout
module-specific log formats
module-specific log files unless explicitly required
```

## 29.13 Event-driven first

Cursor should prefer:

```text
interrupts
subscriptions
snapshots + deltas
systemd/D-Bus signals
socket events
```

Avoid polling unless:

```text
hardware requires it
external software requires it
it is explicitly documented as fallback reconciliation
```

If polling is used, it must be documented with:

```text
reason
interval
owner
expected overhead
failure behavior
```

## 29.14 Systemd dependencies

Cursor must avoid unnecessary `Requires=` relationships.

Prefer:

```ini
Wants=
After=
```

Use `Requires=` only when the service cannot safely start without the dependency.

Services should start, detect unavailable capabilities, report degraded state, and continue where safe.

## 29.15 Backend role

Cursor must keep backend as an API gateway.

The backend should not own:

```text
hardware
audio routing policy
sensor interrupt handling
HomeKit accessory internals
PCM routing
Hi-Fi2 protocol formatting
```

The backend should:

```text
call module facade sockets
normalize errors
serve UI snapshots
subscribe to broker events
read health summaries
```

## 29.16 Frontend role

Cursor must make the frontend show:

```text
module status
capability status
meaningful user messages
what still works
available actions
```

Do not show generic failure messages when the service returned a specific `userMessage`.

## 29.17 Service registry required

Any new service must have a service registry entry.

The entry must include:

```text
service name
module
systemd unit
role
command socket if applicable
criticality
capabilities affected
user-facing failure category
```

## 29.18 Tests and verification

Cursor should add or update tests for:

```text
socket message validation
error envelope validation
broker offline behavior
module facade fallback behavior
health snapshot mapping
service registry validation
```

At minimum, architecture changes must include a verification checklist.

## 29.19 C++ native service rules

For native services:

```text
Use efficient non-blocking IO where appropriate.
Avoid unnecessary allocations in hot paths.
Use bounded queues.
Disconnect slow clients.
Rate-limit repeated logs.
Use RAII for file descriptors and sockets.
Handle SIGTERM/SIGINT cleanly.
Notify systemd watchdog only when actually healthy.
```

## 29.20 TypeScript/Node rules

For TypeScript services:

```text
Use ES modules.
Use strict types.
Use async/await.
Validate external input.
Use schema-driven contracts.
Use explicit timeouts for socket calls.
Never leave unhandled promise rejections.
Log with correlation IDs.
```

## 29.21 No broad rewrites without contract migration

Cursor must not rewrite multiple modules at once without a migration plan.

Preferred sequence:

```text
define contract
add service registry entry
add socket client/server
add health snapshot
migrate backend call path
add broker event publishing
remove old event-bus command path
update docs
update verification
```

## 29.22 Document every intentional exception

If a service cannot follow these rules, Cursor must document:

```text
exception
reason
owner
risk
fallback behavior
future migration path
```

---

# 30. Cursor Implementation Checklist

When Cursor implements this architecture, use this checklist.

## New service checklist

```text
[ ] Service has a documented owner.
[ ] Service has a systemd unit.
[ ] Service has a service registry entry.
[ ] Service has a command socket if it accepts commands.
[ ] Socket path follows /run/homepi/<module>/<service>.sock.
[ ] Socket permissions are 0660 with group homepi.
[ ] Request/response envelope is schema validated.
[ ] Errors use the HomePi error envelope.
[ ] Logs use the HomePi structured logging contract.
[ ] Service has getHealth or snapshot where appropriate.
[ ] Service handles SIGTERM/SIGINT cleanly.
[ ] Service failure does not disrupt unrelated modules.
```

## Broker usage checklist

```text
[ ] Broker only publishes/subscribes events.
[ ] No commands are routed through broker.
[ ] Broker outage does not break direct commands.
[ ] Events use homepi.<module>.<entity>.<event> topics.
[ ] Internal noisy details are not published globally.
[ ] Module facade normalizes public events.
```

## Audio service checklist

```text
[ ] homepi-audio is the facade.
[ ] homepi-hifi owns Hi-Fi2 typed commands.
[ ] Raw Hi-Fi2 strings are not scattered.
[ ] homepi-pcm-router owns only PCM data plane.
[ ] homepi-airplay-manager owns Shairport lifecycle.
[ ] homepi-audio-paging owns paging lifecycle.
[ ] homepi-metadata owns metadata and realtime telemetry.
[ ] Audio reports capability-level status.
[ ] PCM failure does not stop zone control.
[ ] Paging failure does not stop normal audio.
```

## Health checklist

```text
[ ] Health distinguishes process/readiness/domain.
[ ] systemd active does not automatically mean feature ready.
[ ] D-Bus/systemd is used for lifecycle observation.
[ ] Service snapshots are used for domain readiness.
[ ] UI receives meaningful user messages.
```

---

# 31. Final Target Architecture

```text
                         ┌──────────────────────┐
                         │      HomePi UI        │
                         └──────────┬───────────┘
                                    │
                                    ▼
                         ┌──────────────────────┐
                         │   homepi-backend      │
                         │   API + UI gateway    │
                         └──────────┬───────────┘
                                    │
             ┌──────────────────────┼──────────────────────┐
             │                      │                      │
             ▼                      ▼                      ▼
   ┌──────────────────┐   ┌──────────────────┐   ┌──────────────────┐
   │  homepi-audio     │   │ homepi-sensors    │   │ homepi-homekit    │
   │  audio.sock       │   │ sensors.sock      │   │ homekit.sock      │
   └───────┬──────────┘   └───────┬──────────┘   └───────┬──────────┘
           │                      │                      │
           ▼                      ▼                      ▼
   ┌──────────────────┐   ┌──────────────────┐   ┌──────────────────┐
   │ hifi / pcm /      │   │ GPIO / MCP23017   │   │ HAP-NodeJS       │
   │ airplay / paging  │   │ event handling    │   │ bridge           │
   └──────────────────┘   └──────────────────┘   └──────────────────┘

             ┌────────────────────────────────────────────┐
             │              homepi-broker                  │
             │      optional normalized event fanout        │
             └────────────────────────────────────────────┘

             ┌────────────────────────────────────────────┐
             │              homepi-health                  │
             │      systemd/D-Bus health and diagnostics    │
             └────────────────────────────────────────────┘
```

---

# 32. Final Design Statement

HomePi should be a modular Raspberry Pi automation platform where each module owns its runtime behavior, each service has a direct command path, and every failure is isolated to the smallest possible capability.

The broker should improve realtime updates, but it should never be required for commands.

The health service should explain what is broken, what still works, and what action the user can take.

The system should prefer event-driven behavior, direct sockets, structured contracts, and meaningful degraded states over central dependencies and generic failure handling.
