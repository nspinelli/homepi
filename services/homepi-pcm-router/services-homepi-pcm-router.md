# HomePi PCM Router — Hardened Cursor Build Specification

**Document purpose:**  
This document is the source-of-truth build specification for Cursor to create a hardened C application named:

```text
homepi-pcm-router
```

The application manages **16 numeric PCM zones** and routes exactly one selected PCM stream to one assigned physical USB Audio DAC.

The first production source type is Shairport Sync, but the daemon name and internal architecture are intentionally generic so future HomePi sources can reuse the same PCM routing engine.

---

## 1. Official References

Cursor must treat these upstream projects as authoritative references:

| Component | Purpose | Reference |
|---|---|---|
| Shairport Sync | AirPlay / AirPlay 2 receiver | https://github.com/mikebrady/shairport-sync |
| Shairport Sync MQTT | MQTT state and metadata output | https://github.com/mikebrady/shairport-sync/blob/master/MQTT.md |
| nqptp | AirPlay 2 timing helper for Shairport Sync | https://github.com/mikebrady/nqptp |
| Shairport Sync Metadata Reader | Reference parser for Shairport metadata stream | https://github.com/mikebrady/shairport-sync-metadata-reader |
| ALSA loopback driver | Kernel PCM loopback device | https://www.alsa-project.org/wiki/Matrix:Module-aloop |
| Mosquitto | MQTT broker | https://mosquitto.org/ |

---

## 2. Core Architecture

### 2.1 Audio Flow

```text
Apple AirPlay Sender
  → Shairport Sync Zone N
  → ALSA snd-aloop playback substream
  → ALSA snd-aloop capture substream
  → homepi-pcm-router
  → Assigned physical USB Audio DAC
```

### 2.2 Metadata and Control Flow

```text
Shairport Sync Zone N
  → MQTT topic event
  → Mosquitto broker
  → homepi-pcm-router
  → normalized JSON event
  → HomePi UNIX domain socket
  → HomePi backend
  → existing HomePi WebSocket server
  → HomePi UI
```

### 2.3 Scope

`homepi-pcm-router` is responsible for:

- subscribing to Shairport Sync MQTT events
- maintaining authoritative zone state for zones 1–16
- maintaining an active-owner stack
- selecting exactly one zone as DAC owner
- reading/draining all loopback capture streams
- forwarding only the selected owner stream to the assigned DAC
- applying software fade/crossfade during owner changes
- managing the 5-minute DAC idle keepalive
- publishing normalized JSON events to HomePi over a UNIX domain socket
- recovering from MQTT, HomePi socket, and ALSA errors without destabilizing audio

`homepi-pcm-router` is NOT responsible for:

- being an MQTT broker
- serving frontend WebSockets
- authenticating browser clients
- implementing AirPlay
- implementing PTP timing
- replacing nqptp
- mixing zones together
- controlling HomePi frontend sessions
- claiming unrelated USB Audio DACs

### 2.4 Generic Naming Rule

Use generic internal concepts:

```text
source
zone
owner
route
pcm_capture
dac_output
```

Shairport-specific logic belongs only in:

```text
mqtt_client.c
shairport_metadata_adapter.c
```

The audio router must remain generic enough to support future sources such as:

- TTS
- alerts
- doorbell audio
- local audio
- other HomePi modules

---

## 3. Correct Component Roles

### 3.1 Shairport Sync

Each zone runs one Shairport Sync instance.

Each instance:

- advertises one AirPlay target
- receives audio from Apple devices
- publishes MQTT events
- writes PCM audio to its assigned ALSA loopback playback substream

Shairport Sync must never write directly to the physical DAC.

### 3.2 nqptp

`nqptp` runs as an external service.

It supports Shairport Sync timing for AirPlay 2 operation.

The PCM router must not:

- control nqptp
- replace nqptp
- implement PTP
- own the AirPlay master clock

The router only consumes PCM that Shairport Sync has already produced.

### 3.3 Mosquitto

Mosquitto is the MQTT broker.

Shairport Sync instances publish to Mosquitto.

`homepi-pcm-router` subscribes to Mosquitto.

The router must not act as a broker.

### 3.4 HomePi Backend

The HomePi backend remains the single frontend WebSocket server.

The router connects to HomePi through an internal UNIX domain socket and publishes normalized JSON events.

The router must not expose a browser-facing WebSocket server.

---

## 4. Zone Numbering Rules

All zone references must use numeric IDs only:

```text
1, 2, 3, ..., 16
```

Do not use room names such as kitchen, patio, or living room.

Canonical forms:

| Meaning | Canonical Value |
|---|---|
| Zone ID as integer | `1` through `16` |
| Zone ID as string | `"1"` through `"16"` |
| Shairport instance name | `homepi-shairport@1` through `homepi-shairport@16` |
| MQTT topic base | `shairport/zone/1` through `shairport/zone/16` |
| Zones 1–8 playback | `hw:HomePiZonesA,0,<zone_id - 1>` |
| Zones 1–8 capture | `hw:HomePiZonesA,1,<zone_id - 1>` |
| Zones 9–16 playback | `hw:HomePiZonesB,0,<zone_id - 9>` |
| Zones 9–16 capture | `hw:HomePiZonesB,1,<zone_id - 9>` |

Daemon name:

```text
homepi-pcm-router
```

Systemd service name:

```text
homepi-pcm-router.service
```

Recommended generic event names:

```text
pcm.owner.changed
pcm.owner.cleared
pcm.zone.updated
pcm.metadata.updated
pcm.dac.state
pcm.route.changed
pcm.health
```

Shairport-specific details should be represented as source metadata:

```json
{
  "sourceType": "shairport",
  "zoneId": 9
}
```

---

## 5. ALSA Loopback Design

### 5.1 Required ALSA Driver

Use:

```text
snd-aloop
```

Do not use:

- `dmix`
- `amix`
- PulseAudio mixing
- PipeWire mixing
- JACK routing
- software mixer plugins

This system is a router, not a mixer.

Exactly one selected zone is routed to the assigned physical DAC at a time.

### 5.2 Why Two Loopback Cards Are Required for 16 Zones

The ALSA `snd-aloop` loopback driver exposes two cross-connected PCM devices per card:

```text
device 0 playback → device 1 capture
device 1 playback → device 0 capture
```

Each loopback PCM device supports up to 8 substreams.

Because the system requires 16 independent zone streams, use two loopback cards:

```text
HomePiZonesA = zones 1–8
HomePiZonesB = zones 9–16
```

Each card uses:

```text
pcm_substreams=8
```

This creates 16 independent Shairport playback substreams and 16 matching router capture substreams.

### 5.3 Required Loopback Card Names

Use exactly these ALSA loopback card IDs:

```text
HomePiZonesA
HomePiZonesB
```

### 5.4 Zone-to-Loopback Mapping

Each Shairport instance writes to device 0.

The PCM router reads from device 1.

#### Zones 1–8

| Zone | Substream | Shairport Playback Device | PCM Router Capture Device |
|---:|---:|---|---|
| 1 | 0 | `hw:HomePiZonesA,0,0` | `hw:HomePiZonesA,1,0` |
| 2 | 1 | `hw:HomePiZonesA,0,1` | `hw:HomePiZonesA,1,1` |
| 3 | 2 | `hw:HomePiZonesA,0,2` | `hw:HomePiZonesA,1,2` |
| 4 | 3 | `hw:HomePiZonesA,0,3` | `hw:HomePiZonesA,1,3` |
| 5 | 4 | `hw:HomePiZonesA,0,4` | `hw:HomePiZonesA,1,4` |
| 6 | 5 | `hw:HomePiZonesA,0,5` | `hw:HomePiZonesA,1,5` |
| 7 | 6 | `hw:HomePiZonesA,0,6` | `hw:HomePiZonesA,1,6` |
| 8 | 7 | `hw:HomePiZonesA,0,7` | `hw:HomePiZonesA,1,7` |

#### Zones 9–16

| Zone | Substream | Shairport Playback Device | PCM Router Capture Device |
|---:|---:|---|---|
| 9 | 0 | `hw:HomePiZonesB,0,0` | `hw:HomePiZonesB,1,0` |
| 10 | 1 | `hw:HomePiZonesB,0,1` | `hw:HomePiZonesB,1,1` |
| 11 | 2 | `hw:HomePiZonesB,0,2` | `hw:HomePiZonesB,1,2` |
| 12 | 3 | `hw:HomePiZonesB,0,3` | `hw:HomePiZonesB,1,3` |
| 13 | 4 | `hw:HomePiZonesB,0,4` | `hw:HomePiZonesB,1,4` |
| 14 | 5 | `hw:HomePiZonesB,0,5` | `hw:HomePiZonesB,1,5` |
| 15 | 6 | `hw:HomePiZonesB,0,6` | `hw:HomePiZonesB,1,6` |
| 16 | 7 | `hw:HomePiZonesB,0,7` | `hw:HomePiZonesB,1,7` |

### 5.5 Mapping Formula

```text
if zone_id >= 1 and zone_id <= 8:
    loopback_card = "HomePiZonesA"
    substream = zone_id - 1

if zone_id >= 9 and zone_id <= 16:
    loopback_card = "HomePiZonesB"
    substream = zone_id - 9
```

Playback device:

```text
hw:<loopback_card>,0,<substream>
```

Capture device:

```text
hw:<loopback_card>,1,<substream>
```

### 5.6 ALSA Parameter Rule

`snd-aloop` does not perform format, rate, or channel conversion.

Therefore:

- Shairport output format and router capture format must match
- all zones must use the same format
- the router must detect and reject mismatched formats
- the router must not resample in the audio path
- the router must not silently change sample rate, channel count, or bit depth

---

## 6. Step-by-Step ALSA Loopback Setup

This section must be included in the project README and install documentation.

### 6.1 Install ALSA Utilities

```bash
sudo apt-get update
sudo apt-get install -y alsa-utils
```

### 6.2 Create Module-Load File

Create:

```text
/etc/modules-load.d/homepi-aloop.conf
```

Command:

```bash
sudo tee /etc/modules-load.d/homepi-aloop.conf >/dev/null <<'EOF'
snd-aloop
EOF
```

This ensures `snd-aloop` loads at boot.

### 6.3 Create Modprobe Configuration

Create:

```text
/etc/modprobe.d/homepi-aloop.conf
```

Command:

```bash
sudo tee /etc/modprobe.d/homepi-aloop.conf >/dev/null <<'EOF'
options snd-aloop index=10,11 id=HomePiZonesA,HomePiZonesB enable=1,1 pcm_substreams=8,8
EOF
```

This creates:

```text
card 10: HomePiZonesA with 8 substreams
card 11: HomePiZonesB with 8 substreams
```

### 6.4 Load the Driver Immediately

If `snd-aloop` is not in use:

```bash
sudo modprobe -r snd-aloop 2>/dev/null || true
sudo modprobe snd-aloop
```

If it is in use, reboot instead:

```bash
sudo reboot
```

### 6.5 Verify Loopback Cards Exist

Run:

```bash
cat /proc/asound/cards
```

Expected:

```text
10 [HomePiZonesA  ]: Loopback - HomePiZonesA
11 [HomePiZonesB  ]: Loopback - HomePiZonesB
```

Also run:

```bash
aplay -l
arecord -l
```

Expected cards:

```text
card 10: HomePiZonesA, device 0: Loopback PCM
card 10: HomePiZonesA, device 1: Loopback PCM
card 11: HomePiZonesB, device 0: Loopback PCM
card 11: HomePiZonesB, device 1: Loopback PCM
```

### 6.6 Validate Substreams

Simple `aplay -l` output may not list every substream.

The install script must validate each substream by opening it with the configured PCM parameters.

For zones 1–8:

```bash
for i in 0 1 2 3 4 5 6 7; do
  echo "Zone $((i+1)) playback: hw:HomePiZonesA,0,$i"
  aplay -D "hw:HomePiZonesA,0,$i" --dump-hw-params /dev/zero 2>/tmp/homepi-zone-check.log || true
done
```

For zones 9–16:

```bash
for i in 0 1 2 3 4 5 6 7; do
  echo "Zone $((i+9)) playback: hw:HomePiZonesB,0,$i"
  aplay -D "hw:HomePiZonesB,0,$i" --dump-hw-params /dev/zero 2>/tmp/homepi-zone-check.log || true
done
```

Cursor should implement a safer validation utility that opens each playback/capture device with the configured PCM parameters and immediately closes it.

### 6.7 Test One Loopback Pair Manually

Terminal 1 — capture Zone 1:

```bash
arecord -D hw:HomePiZonesA,1,0 -f S32_LE -r 48000 -c 2 /tmp/zone1-test.wav
```

Terminal 2 — play into Zone 1 loopback:

```bash
speaker-test -D hw:HomePiZonesA,0,0 -c 2 -r 48000 -F S32_LE -t sine
```

Stop both after a few seconds.

Inspect:

```bash
file /tmp/zone1-test.wav
```

### 6.8 Configure Shairport Zone Output Devices

Examples:

```conf
// Zone 1
alsa =
{
  output_device = "hw:HomePiZonesA,0,0";
};
```

```conf
// Zone 8
alsa =
{
  output_device = "hw:HomePiZonesA,0,7";
};
```

```conf
// Zone 9
alsa =
{
  output_device = "hw:HomePiZonesB,0,0";
};
```

```conf
// Zone 16
alsa =
{
  output_device = "hw:HomePiZonesB,0,7";
};
```

### 6.9 Configure PCM Router Capture Devices

The router must open:

```text
Zone 1  capture: hw:HomePiZonesA,1,0
Zone 2  capture: hw:HomePiZonesA,1,1
Zone 3  capture: hw:HomePiZonesA,1,2
Zone 4  capture: hw:HomePiZonesA,1,3
Zone 5  capture: hw:HomePiZonesA,1,4
Zone 6  capture: hw:HomePiZonesA,1,5
Zone 7  capture: hw:HomePiZonesA,1,6
Zone 8  capture: hw:HomePiZonesA,1,7
Zone 9  capture: hw:HomePiZonesB,1,0
Zone 10 capture: hw:HomePiZonesB,1,1
Zone 11 capture: hw:HomePiZonesB,1,2
Zone 12 capture: hw:HomePiZonesB,1,3
Zone 13 capture: hw:HomePiZonesB,1,4
Zone 14 capture: hw:HomePiZonesB,1,5
Zone 15 capture: hw:HomePiZonesB,1,6
Zone 16 capture: hw:HomePiZonesB,1,7
```

### 6.10 Troubleshooting

If `HomePiZonesA` or `HomePiZonesB` does not appear:

```bash
lsmod | grep snd_aloop
dmesg | grep -i aloop
cat /proc/asound/cards
```

If cards appear with different names, unload and reload after confirming no process is using them:

```bash
sudo lsof /dev/snd/*
sudo modprobe -r snd-aloop
sudo modprobe snd-aloop
```

If unloading fails, reboot.

---

## 7. Audio Format Rules

The system must use a single fixed PCM format across all zones.

The exact format must be configurable because Shairport Sync behavior can differ depending on AirPlay mode, build options, and source.

Recommended defaults:

```text
HOMEPI_AUDIO_RATE=48000
HOMEPI_AUDIO_CHANNELS=2
HOMEPI_AUDIO_FORMAT=S32_LE
```

Acceptable alternate profile for classic AirPlay:

```text
HOMEPI_AUDIO_RATE=44100
HOMEPI_AUDIO_CHANNELS=2
HOMEPI_AUDIO_FORMAT=S32_LE
```

Cursor must not hard-code a single rate without config.

The router must validate at startup:

- configured rate
- configured channels
- configured sample format
- assigned DAC supports the configured format
- every loopback capture stream opens with the configured format

If the assigned DAC does not support the configured format, startup must fail clearly.

Do not silently resample.

Do not silently downmix.

Do not silently change bit depth.

---

## 8. Physical USB DAC Isolation Rules

The HomePi system may have more than one USB Audio DAC attached.

The router must control only the specific USB Audio DAC assigned to PCM routing.

The router must not block, open, probe destructively, or claim unrelated USB Audio DACs.

ALSA device ownership is per PCM device. Keeping the assigned DAC open only owns that specific configured PCM device. It must not prevent other programs from using a different USB Audio DAC.

Example intended layout:

```text
USB DAC A:
  assigned to homepi-pcm-router
  configured as ALSA_DAC_DEVICE=hw:HomePiShairportDAC,0

USB DAC B:
  available for other audio
  not opened by homepi-pcm-router
```

### 8.1 Required DAC Selection Rule

The router must open only the explicitly configured DAC device:

```env
ALSA_DAC_DEVICE=hw:HomePiShairportDAC,0
```

The router must never automatically open:

```text
default
plughw:default
sysdefault
front
surround*
hw:0,0
hw:1,0
```

unless the exact value was explicitly configured by the operator.

Cursor must not implement fallback logic that silently chooses another DAC if the configured DAC is missing.

Incorrect:

```text
Configured DAC missing
→ router falls back to default ALSA device
→ wrong USB DAC gets claimed
```

Correct:

```text
Configured DAC missing
→ router fails startup with a clear error
→ no other DAC is opened
```

### 8.2 Stable ALSA Naming Requirement

Do not rely on volatile ALSA card numbers such as:

```text
hw:1,0
hw:2,0
```

USB card numbers can change after:

- reboot
- USB reconnect
- hub enumeration changes
- adding/removing another audio device
- kernel update

Use a stable ALSA card ID or udev-assigned identity.

Preferred configured form:

```env
ALSA_DAC_DEVICE=hw:HomePiShairportDAC,0
```

Production config must use a stable name.

### 8.3 DAC Discovery and Validation

The install script may inspect available ALSA devices, but must not choose one automatically unless explicitly configured.

Helpful commands:

```bash
aplay -l
cat /proc/asound/cards
ls -l /proc/asound/by-id 2>/dev/null || true
```

Startup validation must check:

```text
1. ALSA_DAC_DEVICE is set
2. ALSA_DAC_DEVICE is not "default"
3. ALSA_DAC_DEVICE is not empty
4. configured DAC can be opened for playback
5. configured DAC supports required format/rate/channels
6. no fallback DAC is opened if validation fails
```

### 8.4 Keepalive Scope

The 5-minute DAC idle keepalive applies only to the assigned DAC.

When the assigned DAC is in keepalive mode:

```text
hw:HomePiShairportDAC,0 remains open
silence is written
other USB DACs remain unaffected
```

When keepalive expires:

```text
hw:HomePiShairportDAC,0 is closed
other USB DACs were never affected
```

---

## 9. Shairport Sync Zone Configuration

Each zone should have its own Shairport Sync config file.

Example paths:

```text
/etc/shairport-sync/zone-1.conf
/etc/shairport-sync/zone-2.conf
...
/etc/shairport-sync/zone-16.conf
```

Example Zone 1 config:

```conf
general =
{
  name = "HomePi Zone 1";
};

sessioncontrol =
{
  active_state_timeout = 5.0;
};

alsa =
{
  output_device = "hw:HomePiZonesA,0,0";
};

mqtt =
{
  enabled = "yes";
  hostname = "127.0.0.1";
  port = 1883;
  topic = "shairport/zone/1";
  publish_raw = "no";
  publish_parsed = "yes";
  publish_cover = "no";
};
```

Example Zone 16 config:

```conf
general =
{
  name = "HomePi Zone 16";
};

sessioncontrol =
{
  active_state_timeout = 5.0;
};

alsa =
{
  output_device = "hw:HomePiZonesB,0,7";
};

mqtt =
{
  enabled = "yes";
  hostname = "127.0.0.1";
  port = 1883;
  topic = "shairport/zone/16";
  publish_raw = "no";
  publish_parsed = "yes";
  publish_cover = "no";
};
```

### 9.1 Shairport Build Requirements

Shairport Sync must be built with:

- ALSA support
- MQTT client support
- metadata support as needed
- AirPlay 2 support if using AirPlay 2 zones

Validation command:

```bash
shairport-sync -V
```

---

## 10. MQTT Topic Model

### 10.1 Subscription

The router subscribes to:

```text
shairport/zone/+/+
```

Example topics:

```text
shairport/zone/1/active_start
shairport/zone/1/active_end
shairport/zone/1/title
shairport/zone/1/artist
shairport/zone/1/album
shairport/zone/1/client_name
shairport/zone/1/client_ip
shairport/zone/1/client_model
shairport/zone/1/volume
shairport/zone/16/active_start
```

### 10.2 Topic Parsing

Given:

```text
topic   = shairport/zone/3/title
payload = Everlong
```

Parse as:

```text
base    = shairport
type    = zone
zone_id = 3
field   = title
value   = Everlong
```

Invalid topics must be ignored with rate-limited warning logs.

### 10.3 MQTT Event Semantics

`active_start` means the Shairport instance entered an active AirPlay session.

`active_end` means the active state ended after Shairport Sync's configured timeout.

`active_start` and `active_end` are the main signals for zone stack membership.

Metadata topics are incremental field updates.

The router must not wait for a perfect metadata snapshot because metadata fields arrive independently.

### 10.4 Active Stack Rules

The router maintains:

```c
int active_stack[16];
size_t active_count;
```

Rules:

```text
On active_start(zone_id):
  - reject zone_id outside 1–16
  - remove zone_id from active_stack if already present
  - insert zone_id at index 0
  - mark zone active
  - select zone as DAC owner
  - cancel DAC idle keepalive if running
  - emit pcm.owner.changed
  - emit pcm.zone.updated

On active_end(zone_id):
  - reject zone_id outside 1–16
  - mark zone inactive
  - remove zone_id from active_stack
  - if stack still has zones:
      select active_stack[0] as DAC owner
      crossfade to fallback owner
      emit pcm.owner.changed
    else:
      clear owner
      start DAC idle keepalive
      write silence
      emit pcm.owner.cleared
```

Example:

```text
Zone 1 active_start
active_stack = [1]
owner = 1

Zone 12 active_start
active_stack = [12, 1]
owner = 12

Zone 16 active_start
active_stack = [16, 12, 1]
owner = 16

Zone 16 active_end
active_stack = [12, 1]
owner = 12
```

### 10.5 MQTT Failure Rule

MQTT failure must never stop audio.

If MQTT disconnects:

- keep routing current owner
- keep draining loopbacks
- keep DAC lifecycle active
- attempt reconnect with backoff
- emit degraded status to HomePi when possible

---

## 11. Metadata Snapshot Model

Shairport MQTT metadata is field-based, not a complete JSON object.

The router builds its own authoritative state object.

### 11.1 Zone State Example

```c
typedef struct {
  int zone_id;

  bool active;
  bool playing;

  bool has_title;
  bool has_artist;
  bool has_album;
  bool has_client_name;
  bool has_client_ip;
  bool has_client_model;
  bool has_volume;

  char title[256];
  char artist[256];
  char album[256];
  char client_name[128];
  char client_ip[64];
  char client_model[128];
  char volume[64];

  uint64_t last_mqtt_event_ms;
  uint64_t last_metadata_update_ms;
  bool metadata_dirty;
} ZoneState;
```

### 11.2 Metadata Debounce

Recommended behavior:

```text
On owner change:
  emit immediate owner snapshot with current known metadata

On metadata field update:
  update zone state
  if zone is current owner:
      start/restart 150–300 ms debounce timer

When debounce timer expires:
  emit pcm.metadata.updated
```

Default:

```text
METADATA_DEBOUNCE_MS=250
```

### 11.3 Snapshot Completeness

Do not define complete as “all fields received.”

Use:

```text
empty    = no useful metadata known
partial  = at least one useful field known
stable   = debounce window elapsed
```

Example:

```json
{
  "type": "pcm.metadata.updated",
  "version": 1,
  "timestampMs": 1770000000000,
  "source": "homepi-pcm-router",
  "payload": {
    "sourceType": "shairport",
    "ownerZoneId": 3,
    "metadataStatus": "stable",
    "metadata": {
      "title": "Everlong",
      "artist": "Foo Fighters",
      "album": "The Colour and the Shape",
      "clientName": "Nicholas's iPhone",
      "clientIp": "10.3.3.44",
      "clientModel": "iPhone",
      "volume": "-12.0"
    }
  }
}
```

---

## 12. PCM Routing Design

### 12.1 Hard Rule

Only the DAC writer thread writes to the physical DAC.

No other thread may call `snd_pcm_writei()` or `snd_pcm_writen()` on the DAC handle.

### 12.2 Capture Threads

Create one capture/drain thread per zone.

For 16 zones, this means 16 capture threads plus one DAC writer thread.

Each capture thread:

- opens its assigned loopback capture device
- continuously reads PCM frames
- writes frames into the zone ringbuffer if the zone may be selected
- drains/discards frames when the zone is not selected
- never blocks Shairport Sync
- never writes to the DAC directly

### 12.3 Ringbuffer Model

Each zone has a fixed-size ringbuffer:

```text
Zone N loopback capture
  → Zone N ringbuffer
```

DAC writer reads from:

```text
current_owner ringbuffer
```

If no current owner:

```text
silence buffer
```

Requirements:

- fixed allocation at startup
- no malloc/free in audio loop
- lock-free or bounded non-blocking locking
- overrun counter
- underrun counter
- high-water and low-water metrics

### 12.4 Deterministic Routing Algorithm

The PCM router must make a routing decision on every DAC write cycle.

Order of operations:

```text
1. Read current selected owner atomically
2. For each zone capture thread:
     - continue reading from that zone's loopback capture device
     - if zone == selected owner:
         make latest PCM frames available to DAC writer
       else:
         drain/discard frames so Shairport does not block
3. DAC writer pulls from selected owner's ringbuffer
4. If selected owner has no available frames:
     write silence for that period
5. If no selected owner exists:
     write silence during DAC keepalive window
```

The router must never stop reading inactive zones.

The router must never wait for inactive zones.

The router must never allow an inactive zone's buffer to grow unbounded.

### 12.5 Active Owner Selection Comes First

Before deciding whether a zone's PCM is routed or drained, the daemon must determine the selected owner from the authoritative active stack.

```text
selected_owner = active_stack[0] if active_stack is not empty
selected_owner = null if active_stack is empty
```

Routing is based on manager state, not on whichever PCM device produced frames most recently.

Incorrect:

```text
Zone 1 produces frames first
→ route Zone 1 to DAC even though active_stack[0] is Zone 12
```

Correct:

```text
active_stack[0] = Zone 12
Zone 12 frames → DAC
Zone 1 frames → drained/discarded
Zone 2 frames → drained/discarded
...
Zone 16 frames → drained/discarded
```

### 12.6 DAC Writer Behavior

The DAC writer is the only thread that writes to the physical DAC.

On each period:

```text
if dac_state == DAC_CLOSED:
    do not write
else if route_state == ROUTE_CROSSFADE:
    read old owner buffer
    read new owner buffer
    apply fade-out/fade-in
    write mixed result to DAC
else if selected_owner exists:
    read selected owner buffer
    if enough frames:
        write selected frames to DAC
    else:
        write silence and increment underrun counter
else:
    write silence
```

The DAC writer must not read directly from ALSA loopback capture devices.

The DAC writer reads only from zone ringbuffers or silence buffers.

### 12.7 Inactive Zone Drain Example

Assume:

```text
active_stack = [12, 4, 1]
selected_owner = Zone 12
```

Then:

```text
Zone 12 loopback capture → Zone 12 ringbuffer → DAC writer → physical DAC

Zone 1 loopback capture → drained/discarded
Zone 2 loopback capture → drained/discarded
Zone 3 loopback capture → drained/discarded
Zone 4 loopback capture → drained/discarded
Zone 5 loopback capture → drained/discarded
Zone 6 loopback capture → drained/discarded
Zone 7 loopback capture → drained/discarded
Zone 8 loopback capture → drained/discarded
Zone 9 loopback capture → drained/discarded
Zone 10 loopback capture → drained/discarded
Zone 11 loopback capture → drained/discarded
Zone 13 loopback capture → drained/discarded
Zone 14 loopback capture → drained/discarded
Zone 15 loopback capture → drained/discarded
Zone 16 loopback capture → drained/discarded
```

Even though Zone 4 and Zone 1 are still active AirPlay sessions, they must not reach the DAC unless they become `active_stack[0]`.

### 12.8 Suggested Pseudo-Code

```c
void zone_capture_thread(ZoneContext *zone) {
    while (running) {
        snd_pcm_sframes_t frames = snd_pcm_readi(
            zone->capture_handle,
            zone->capture_buffer,
            PERIOD_FRAMES
        );

        if (frames < 0) {
            recover_capture_xrun(zone, frames);
            continue;
        }

        int selected = atomic_load(&router.selected_owner_zone_id);

        if (selected == zone->zone_id) {
            ringbuffer_write_latest(&zone->ringbuffer, zone->capture_buffer, frames);
        } else {
            zone->metrics.drained_frames += frames;
            // Frames intentionally discarded.
            // This keeps Shairport's loopback playback side from blocking.
        }
    }
}

void dac_writer_thread(AudioRouter *router) {
    while (running) {
        if (router->dac_state == DAC_CLOSED) {
            wait_for_dac_open_signal();
            continue;
        }

        int selected = atomic_load(&router->selected_owner_zone_id);

        if (router->route_state == ROUTE_CROSSFADE) {
            render_crossfade_period(router);
        } else if (selected >= 1 && selected <= 16) {
            ZoneContext *zone = &router->zones[selected - 1];

            if (ringbuffer_read(&zone->ringbuffer, router->output_buffer, PERIOD_FRAMES)) {
                snd_pcm_writei(router->dac_handle, router->output_buffer, PERIOD_FRAMES);
            } else {
                write_silence_period(router);
                zone->metrics.output_underruns++;
            }
        } else {
            write_silence_period(router);
        }
    }
}
```

---

## 13. Software Fade and Crossfade

Default:

```text
FADE_MS=10
```

Allowed range:

```text
5–20 ms
```

At 48 kHz:

```text
5 ms  = 240 frames
10 ms = 480 frames
20 ms = 960 frames
```

During owner switch:

```text
old_owner → fade out
new_owner → fade in
mixed output → DAC
```

Pseudo-code:

```c
for each frame i in fade_window:
    float t = (float)i / (float)(fade_frames - 1);
    float old_gain = 1.0f - t;
    float new_gain = t;

    out_left  = old_left  * old_gain + new_left  * new_gain;
    out_right = old_right * old_gain + new_right * new_gain;

    clamp_and_write(out);
```

If old owner has no frames, fade in new owner from silence.

If new owner has no frames, fade out old owner to silence.

Do not block waiting for frames.

---

## 14. DAC Lifecycle and 5-Minute Idle Keepalive

Default:

```text
DAC_IDLE_KEEPALIVE_MS=300000
```

The DAC must stay open during:

- active playback
- owner switching
- the 5-minute idle keepalive window after the final zone leaves

The DAC may close only after:

- active stack is empty
- no zone became active during the keepalive window
- the 5-minute timer expired
- the daemon rechecked that active stack is still empty

State machine:

```text
DAC_CLOSED
  → DAC_OPENING when active_stack becomes non-empty

DAC_OPENING
  → open assigned DAC
  → configure fixed PCM format
  → prepare playback
  → start writer
  → DAC_ACTIVE

DAC_ACTIVE
  → selected owner routes to DAC
  → if active_stack becomes empty: DAC_KEEPALIVE

DAC_KEEPALIVE
  → write silence
  → keep assigned DAC open
  → if active_stack becomes non-empty: DAC_ACTIVE
  → if timer expires and stack still empty: DAC_CLOSING

DAC_CLOSING
  → verify active_stack still empty
  → write final silence frames
  → gracefully stop writer
  → close assigned DAC
  → DAC_CLOSED
```

Loopback drain threads must continue even while the physical DAC is closed.

Closing the DAC must not stop:

- MQTT
- zone state
- loopback drains
- HomePi UNIX socket events
- nqptp
- Shairport instances

---

## 15. NQPTP and Clock Continuity

`nqptp` provides timing support used by Shairport Sync for AirPlay 2.

The PCM router does not consume nqptp directly.

Do not:

- stop Shairport instances during switching
- stop reading loopback capture streams for inactive zones
- let loopback buffers fill
- tie DAC ownership to Shairport process lifetime

Correct model:

```text
All Shairport instances remain running.
All loopbacks are continuously drained.
Only selected PCM is forwarded to DAC.
```

---

## 16. UNIX Domain Socket Publishing

Recommended socket:

```text
/run/homepi/events.sock
```

The router connects as a local client.

The router does not host frontend WebSockets.

Use newline-delimited JSON:

```text
{"type":"pcm.owner.changed",...}\n
{"type":"pcm.zone.updated",...}\n
```

Do not send raw MQTT payloads directly to the UI.

Event envelope:

```json
{
  "type": "pcm.owner.changed",
  "version": 1,
  "timestampMs": 1770000000000,
  "correlationId": "owner-change-12",
  "source": "homepi-pcm-router",
  "payload": {}
}
```

Owner changed example:

```json
{
  "type": "pcm.owner.changed",
  "version": 1,
  "timestampMs": 1770000000000,
  "correlationId": "owner-change-12",
  "source": "homepi-pcm-router",
  "payload": {
    "sourceType": "shairport",
    "ownerZoneId": 12,
    "previousOwnerZoneId": 1,
    "activeStack": [12, 1],
    "reason": "active_start"
  }
}
```

DAC state example:

```json
{
  "type": "pcm.dac.state",
  "version": 1,
  "timestampMs": 1770000000000,
  "correlationId": "dac-state-closed",
  "source": "homepi-pcm-router",
  "payload": {
    "state": "DAC_CLOSED",
    "reason": "idle_keepalive_expired"
  }
}
```

---

## 17. C Project Layout

Recommended layout:

```text
homepi-pcm-router/
├── CMakeLists.txt
├── README.md
├── install.sh
├── config/
│   ├── homepi-pcm-router.env.example
│   ├── shairport-zone-template.conf
│   ├── homepi-aloop.conf
│   └── homepi-aloop-modprobe.conf
├── systemd/
│   └── homepi-pcm-router.service
├── include/
│   ├── audio_dac.h
│   ├── audio_loopback.h
│   ├── audio_router.h
│   ├── config.h
│   ├── dac_lifecycle.h
│   ├── json_events.h
│   ├── log.h
│   ├── mqtt_client.h
│   ├── ringbuffer.h
│   ├── shairport_metadata_adapter.h
│   ├── unix_socket_client.h
│   └── zone_state.h
├── src/
│   ├── audio_dac.c
│   ├── audio_loopback.c
│   ├── audio_router.c
│   ├── config.c
│   ├── dac_lifecycle.c
│   ├── json_events.c
│   ├── log.c
│   ├── main.c
│   ├── mqtt_client.c
│   ├── ringbuffer.c
│   ├── shairport_metadata_adapter.c
│   ├── unix_socket_client.c
│   └── zone_state.c
└── tests/
    ├── test_topic_parse.c
    ├── test_active_stack.c
    ├── test_pcm_mapping.c
    ├── test_pcm_owner_drain.c
    ├── test_dac_lifecycle.c
    └── test_fade_math.c
```

---

## 18. Configuration File

Example:

```env
HOMEPI_ZONE_COUNT=16

MQTT_HOST=127.0.0.1
MQTT_PORT=1883
MQTT_TOPIC_FILTER=shairport/zone/+/+

HOMEPI_EVENT_SOCKET=/run/homepi/events.sock

ALSA_LOOPBACK_CARDS=HomePiZonesA,HomePiZonesB
ALSA_DAC_DEVICE=hw:HomePiShairportDAC,0

HOMEPI_AUDIO_RATE=48000
HOMEPI_AUDIO_CHANNELS=2
HOMEPI_AUDIO_FORMAT=S32_LE

PERIOD_FRAMES=256
BUFFER_FRAMES=1024

FADE_MS=10
METADATA_DEBOUNCE_MS=250
DAC_IDLE_KEEPALIVE_MS=300000

LOG_LEVEL=info
```

Important:

```text
ALSA_DAC_DEVICE must point to the one USB DAC assigned to PCM routing.
Do not set ALSA_DAC_DEVICE to default.
Do not rely on volatile card numbers in production.
The router must fail fast if the configured DAC is missing.
```

---

## 19. Dependency Requirements

Required:

- `alsa-lib`
- `libmosquitto`
- POSIX threads
- C11 or newer

Recommended packages:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  pkg-config \
  libasound2-dev \
  libmosquitto-dev \
  alsa-utils \
  mosquitto \
  mosquitto-clients
```

---

## 20. Systemd Service

Example:

```ini
[Unit]
Description=HomePi PCM Router
After=network-online.target mosquitto.service sound.target
Wants=network-online.target
Requires=mosquitto.service

[Service]
Type=simple
User=homepi
Group=audio
SupplementaryGroups=audio
EnvironmentFile=/opt/homepi/services/pcm-router/env/.env
ExecStart=/usr/local/bin/homepi-pcm-router
Restart=always
RestartSec=1

NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
PrivateDevices=no
MemoryDenyWriteExecute=yes
RestrictRealtime=no
LimitRTPRIO=infinity
LimitMEMLOCK=infinity
CapabilityBoundingSet=CAP_SYS_NICE CAP_IPC_LOCK
AmbientCapabilities=CAP_SYS_NICE CAP_IPC_LOCK

ReadWritePaths=/run /var/log /opt/homepi/services/pcm-router

[Install]
WantedBy=multi-user.target
```

Important:

`PrivateDevices=no` is required because the service needs access to ALSA devices.

`RestrictRealtime=no`, `LimitRTPRIO`, and `CAP_SYS_NICE` are required if using real-time scheduling.

`CAP_IPC_LOCK` is required if using `mlockall()`.

---

## 21. Real-Time Hardening

Audio threads should attempt:

```c
mlockall(MCL_CURRENT | MCL_FUTURE);
```

DAC writer and capture threads should attempt:

```text
SCHED_FIFO
```

If real-time scheduling fails:

- log a warning
- continue running
- do not fail startup unless config requires realtime

Audio thread rules:

- no heap allocation in loops
- no blocking socket writes
- no MQTT calls
- no JSON serialization
- no unbounded logging
- no file I/O
- no DNS
- no system calls that can block unpredictably

---

## 22. Failure Recovery

### 22.1 ALSA Xruns

On capture or playback xrun:

- call `snd_pcm_recover()`
- increment metric counter
- continue
- rate-limit log output

### 22.2 MQTT Disconnect

On MQTT disconnect:

- keep audio running
- reconnect with exponential backoff
- do not clear active stack immediately
- mark MQTT status degraded

### 22.3 HomePi UNIX Socket Disconnect

On UNIX socket disconnect:

- keep audio running
- attempt reconnect
- drop non-critical events if queue is full
- do not block audio threads

### 22.4 Zone Loopback Failure

If a zone capture stream fails:

- mark zone audio degraded
- remove zone from active stack if unrecoverable
- fallback to next active zone
- keep other zones running

### 22.5 Assigned DAC Failure

If the assigned DAC fails:

- attempt recovery
- if recovery fails, transition to DAC_CLOSED / degraded
- do not open another DAC
- keep MQTT and loopback drains running
- emit degraded state when possible

---

## 23. Install Script Requirements

`install.sh` must be idempotent.

It must:

1. build the daemon
2. install binary to `/usr/local/bin/homepi-pcm-router`
3. install config directory
4. install systemd unit
5. install ALSA loopback module-load config
6. install ALSA loopback modprobe config
7. reload systemd
8. load `snd-aloop`
9. validate `HomePiZonesA` exists
10. validate `HomePiZonesB` exists
11. validate all 16 playback substreams are openable
12. validate all 16 capture substreams are openable
13. validate `ALSA_DAC_DEVICE` is explicitly configured
14. validate configured DAC device is openable
15. validate configured DAC is not `default`
16. validate no fallback DAC is selected if configured DAC fails
17. validate Mosquitto is reachable
18. enable and start service

Validation examples:

```bash
aplay -l | grep HomePiZonesA
aplay -l | grep HomePiZonesB
arecord -l | grep HomePiZonesA
arecord -l | grep HomePiZonesB
mosquitto_sub -h 127.0.0.1 -t 'shairport/zone/+/+' -C 1 -W 2
```

---

## 24. Cursor Implementation Rules

Cursor must follow these rules:

1. Build the router in C.
2. Use daemon name `homepi-pcm-router`.
3. Use numeric zones only: 1–16.
4. Do not use room names.
5. Do not create a frontend WebSocket server.
6. Do not implement an MQTT broker.
7. Do not send raw MQTT directly to the UI.
8. Use normalized JSON over a UNIX domain socket.
9. Use `snd-aloop` with two cards and eight substreams each.
10. Do not use `dmix`, `amix`, PulseAudio, PipeWire, or JACK.
11. Keep all 16 loopback capture streams drained.
12. Only the DAC writer thread may write to the assigned physical DAC.
13. Do not close the DAC during switching.
14. Use 5-minute idle keepalive before closing the DAC.
15. Continue loopback drains while the DAC is closed.
16. Do not stop Shairport instances during switching.
17. Do not manage nqptp.
18. Do not perform runtime resampling.
19. Do not perform runtime format conversion.
20. Use fixed-size preallocated buffers.
21. Use rate-limited structured JSON logs.
22. Audio continuity has priority over metadata, MQTT, and UI events.
23. MQTT failure must not stop audio.
24. HomePi socket failure must not stop audio.
25. Metadata may be partial and unordered.
26. Owner changes must be deterministic and stack-based.
27. Apply fade/crossfade during owner switching.
28. PCM routing must select the active owner before deciding whether each zone is routed or drained.
29. A zone may not become DAC owner merely because PCM frames arrived first.
30. All non-owner zones must be drained continuously.
31. The router must open only the configured USB Audio DAC assigned to PCM routing.
32. The router must never auto-fallback to `default` or another DAC.
33. Keeping the assigned DAC open must not affect other USB Audio DACs.
34. Production config must use a stable ALSA DAC name, not volatile card numbers.
35. Write tests for topic parsing, active stack behavior, ALSA mapping, PCM owner/drain routing, DAC lifecycle, DAC selection validation, and fade math.

---

## 25. Acceptance Tests

### 25.1 Topic Parser

Input:

```text
shairport/zone/5/title
```

Expected:

```text
zone_id = 5
field = title
```

Input:

```text
shairport/zone/16/active_start
```

Expected:

```text
zone_id = 16
field = active_start
```

Invalid:

```text
shairport/kitchen/title
shairport/zone/17/title
foo/bar
```

Expected:

```text
rejected
```

### 25.2 ALSA Mapping

Expected:

```text
Zone 1  playback = hw:HomePiZonesA,0,0
Zone 1  capture  = hw:HomePiZonesA,1,0

Zone 8  playback = hw:HomePiZonesA,0,7
Zone 8  capture  = hw:HomePiZonesA,1,7

Zone 9  playback = hw:HomePiZonesB,0,0
Zone 9  capture  = hw:HomePiZonesB,1,0

Zone 16 playback = hw:HomePiZonesB,0,7
Zone 16 capture  = hw:HomePiZonesB,1,7
```

### 25.3 Active Stack

Sequence:

```text
active_start(1)
active_start(12)
active_start(16)
active_end(16)
active_end(12)
active_end(1)
```

Expected owners:

```text
1
12
16
12
1
null
```

### 25.4 PCM Owner Routing

Given:

```text
active_stack = [12, 4, 1]
selected_owner = 12
```

Expected:

```text
Zone 12 frames are eligible for DAC output.
All other zones are drained/discarded.
```

If Zone 1 produces frames before Zone 12:

```text
Zone 1 must still be drained.
Zone 12 remains selected owner.
```

### 25.5 DAC Lifecycle

Sequence:

```text
active_start(1)
active_end(1)
wait 299999 ms
active_start(2)
```

Expected:

```text
DAC remains open
owner = 2
```

Sequence:

```text
active_start(1)
active_end(1)
wait 300000 ms
```

Expected:

```text
DAC closes
loopback drains continue
```

### 25.6 DAC Selection Isolation

Given:

```env
ALSA_DAC_DEVICE=hw:HomePiShairportDAC,0
```

And the system also has:

```text
hw:OtherUsbDAC,0
```

Expected:

```text
router opens only hw:HomePiShairportDAC,0
router never opens hw:OtherUsbDAC,0
other applications can still use hw:OtherUsbDAC,0
```

If configured DAC is missing:

```text
hw:HomePiShairportDAC,0 does not exist
```

Expected:

```text
router fails startup
router does not open default
router does not open hw:OtherUsbDAC,0
router logs clear configuration error
```

### 25.7 Fade Math

For 10 ms at 48 kHz:

```text
fade_frames = 480
```

First frame:

```text
old_gain ≈ 1.0
new_gain ≈ 0.0
```

Last frame:

```text
old_gain ≈ 0.0
new_gain ≈ 1.0
```

---

## 26. Final Reliability Principles

The router must behave like audio appliance firmware.

Most important invariant:

```text
The UI, MQTT, metadata, and logging paths must never block or destabilize PCM routing.
```

Second invariant:

```text
All loopback streams must be continuously drained so Shairport Sync does not block.
```

Third invariant:

```text
Only one selected zone may reach the assigned DAC at a time.
```

Fourth invariant:

```text
The assigned DAC may close only after the 5-minute idle keepalive expires with no active zones.
```

Fifth invariant:

```text
nqptp remains external; the router never owns or recreates AirPlay timing.
```

Sixth invariant:

```text
The router must never claim a USB Audio DAC other than the explicitly configured assigned DAC.
```
