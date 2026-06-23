# HomePi Update

## 1. Purpose

This document captures the architecture decisions for moving HomePi toward a cleaner event-driven audio system with low idle cost, strict service boundaries, pipe-only metadata parsing, and realtime UI updates that do not overload the core event bus.

The goals are:

1. Use one logical internal socket bus for HomePi control-plane messages.
2. Keep PCM audio, raw metadata, album art bytes, and high-frequency progress off the event bus.
3. Move orchestration out of Shairport hook scripts.
4. Make disabled zones consume no audio runtime resources.
5. Allow zones to be enabled/disabled from the frontend without restarting the PCM router service.
6. Use lazy PCM capture so enabled idle zones stay lightweight.
7. Use a C++20 `homepi-metadata` daemon that reads Shairport metadata pipes only.
8. Send meaningful metadata state changes through `core/events`.
9. Send live playback telemetry through a dedicated latest-value realtime socket owned by `homepi-metadata`.
10. Persist current now-playing state and the last 20 played streams through `core/storage`.
11. Log every native service through `core/logging` only.

---

## 2. Primary Architecture Decision

HomePi should use one shared application event bus for control-plane messages:

```text
/run/homepi/events.sock
```

All normal service-to-service commands, state changes, lifecycle transitions, and snapshots should go through this broker. Services should not directly command each other through service-specific sockets during normal operation.

The broker carries small, durable-ish state messages:

```text
allowed over core/events:
  shairport active_begin / play_begin / play_end
  pcm route_start / route_end commands
  pcm owner_changed / owner_pending / zone_capture_opened
  hifi typed zone commands
  hifi confirmed zone state
  zone enabled/disabled changes
  metadata owner/track/cover/history state changes
  service lifecycle and health state

not allowed over core/events:
  PCM frames
  raw Shairport metadata XML
  album art binary blobs
  high-rate audio data
  periodic playback position ticks
  latest-value telemetry frames
```

The actual audio path remains outside the event bus:

```text
Shairport Sync
  -> ALSA loopback playback side
  -> homepi-pcm-router capture side
  -> DAC playback
  -> Hi-Fi2 source input
  -> speaker zone
```

The event bus controls the state machine around that path. It does not carry the audio or realtime telemetry.

---

## 3. Core Service Boundaries

### 3.1 `core/transport`

`core/transport` owns reusable local transport mechanics.

Responsibilities:

```text
- Unix socket server/client helpers
- SOCK_SEQPACKET helpers where supported
- SOCK_STREAM + NDJSON or length-prefixed fallback helpers
- non-blocking read/write helpers
- epoll registration helper types
- message size limits
- socket permissions helpers
- reconnect/backoff helpers
- bounded output queues
- latest-value publisher utility
- slow-client disconnect utilities
- integration with core/logging without owning log formatting
```

`core/transport` must not know about:

```text
- AirPlay
- Shairport Sync
- PCM routing
- Hi-Fi2
- zones
- track metadata
- cover art
- playback progress
- UI media cards
```

#### Realtime Socket Ownership Boundary

`core/transport` must **not** own `/run/homepi/audio-realtime.sock` as a running endpoint.

The ownership split is:

```text
core/transport owns:
  how local sockets work
  how frames are encoded/decoded
  how non-blocking IO is handled
  how epoll registrations are represented
  how latest-value queues overwrite stale frames
  how slow clients are detected/disconnected
  how socket permissions are applied

homepi-metadata owns:
  /run/homepi/audio-realtime.sock lifecycle
  audio realtime payload schema
  playback progress model
  now-playing realtime snapshot
  backend subscription behavior
  when snapshots/corrections are produced
  what fields are exposed to backend/UI
```

Correct dependency direction:

```text
homepi-metadata
  -> uses core/transport
  -> owns audio realtime endpoint

core/transport
  -> knows nothing about audio, metadata, zones, AirPlay, or progress
```

The service manifest declares the socket for discovery and permissions, but ownership remains with `homepi-metadata`:

```json
{
  "service": "homepi-metadata",
  "sockets": {
    "audioRealtime": {
      "path": "/run/homepi/audio-realtime.sock",
      "type": "unix_seqpacket",
      "owner": "homepi-metadata",
      "group": "homepi",
      "permissions": "0660",
      "protocol": "homepi.audio.realtime.v1",
      "implementedWith": "core/transport"
    }
  }
}
```

### 3.2 `core/events`

`core/events` owns the HomePi broker.

Responsibilities:

```text
- bind /run/homepi/events.sock
- accept persistent service connections
- service registration
- topic subscriptions
- message validation
- publish/fanout
- request/reply correlation
- source/topic permission checks
- service presence tracking
- retained snapshots where needed
```

`core/events` must not contain domain rules such as:

```text
When AirPlay zone 7 starts, power Hi-Fi2 zone 7 and route PCM zone 7.
```

That belongs in `homepi-audio-orchestrator`.

### 3.3 `core/storage`

`core/storage` owns all SQLite access and migrations.

`homepi-metadata` must persist through `core/storage`. It should not open a separate metadata-specific database unless that database is explicitly owned by the core storage configuration.

Preferred database path:

```text
/opt/homepi/runtime/state/homepi.sqlite
```

### 3.4 `core/logging`

`core/logging` owns the native logging contract.

Responsibilities:

```text
- provide shared native C++ logging API
- define stable log levels and event names
- write one structured JSON log object per line for journald capture
- include service, level, event, message, correlationId, and data fields
- provide rate limiting helpers for repeated warnings/errors
- provide systemd watchdog notification helpers where needed
```

Every native service must use `core/logging` directly.

Do not use:

```text
printf for service logs
std::cout ad-hoc logs
module-specific JSON logger
module-specific log files
ad-hoc syslog calls
```

Normal runtime state changes go through `core/events`. Diagnostics go through `core/logging`.

---

## 4. Domain Service Boundaries

### 4.1 `homepi-audio-orchestrator`

Owns the audio domain state machine.

Responsibilities:

```text
- consume Shairport session events
- maintain active AirPlay zone stack
- decide desired PCM owner
- command PCM router
- command Hi-Fi2 serial service
- command Shairport supervisor for zone lifecycle
- coordinate frontend zone enable/disable
- publish normalized audio state
```

Allowed domain knowledge:

```text
- AirPlay source number = 7
- Hi-Fi2 zones = 1–16
- PCM router has latest-ready-wins owner behavior
- Shairport zones map 1:1 to HomePi/Hi-Fi2 zones
```

### 4.2 `homepi-shairport-supervisor`

Owns Shairport Sync processes and generated config files.

Responsibilities:

```text
- generate zone config files
- start/stop homepi-shairport@N.service
- publish Shairport lifecycle events
- run tiny hook/helper
- never directly command PCM router
- never directly command Hi-Fi2 serial
```

The hook/helper should publish one event to `core/events` and exit. It should not shell out to `nc`, parse PCM state, or send raw Hi-Fi2 commands.

### 4.3 `homepi-pcm-router`

Owns ALSA loopback capture, DAC playback, active stack, and owner selection.

Responsibilities:

```text
- subscribe to modules.pcm.command
- open/close capture handles per zone
- maintain enabled zone mask
- maintain active stack
- promote pending owner only when frames are ready
- drain/buffer active non-owner zones as policy requires
- close disabled zones immediately
- publish owner/state events
```

It should never power Hi-Fi2 zones directly.

### 4.4 `homepi-hifi-serial`

Owns the Hi-Fi2 serial protocol.

Responsibilities:

```text
- subscribe to modules.hifi.command
- expose typed command handlers
- serialize Hi-Fi2 protocol writes
- enforce 50 ms command spacing
- parse controller responses
- publish confirmed controller state
```

Only this service should format raw protocol commands such as:

```text
*Z7POWER1
*Z7SRC7
*Z7ENABLE0
```

Other services must send typed commands.

### 4.5 `homepi-metadata`

Owns Shairport Sync metadata, the global now-playing reducer, realtime playback telemetry, cover art cache, and last-20 play history.

Primary decisions:

```text
- native C++20 daemon
- pipe-only metadata input
- no Python/Node/shell parser
- no Shairport MQTT in the native metadata path
- owner-zone parsing only
- non-owner enabled-zone drain only
- core/events for low-frequency state changes
- /run/homepi/audio-realtime.sock for latest-value progress telemetry
- core/storage for current now-playing and last 20 history
- core/logging for diagnostics
```

---

## 5. Event Message Envelope

All broker messages use a common envelope.

```json
{
  "version": 1,
  "id": "evt-example-001",
  "source": "homepi-example-service",
  "topic": "modules.example.topic",
  "event": "example_event",
  "correlationId": "action-or-session-id",
  "timestamp": "2026-06-20T00:00:00.000Z",
  "payload": {}
}
```

Required fields:

```text
version       integer protocol version
id            unique message id
source        publishing service name
topic         routing topic
event         event/action name
correlationId shared id across one lifecycle/action
timestamp     ISO-8601 UTC timestamp
payload       event-specific object
```

Recommended topic families:

```text
core.broker
core.service
modules.zone.command
modules.zone.config
modules.shairport.command
modules.shairport.session
modules.shairport.volume
modules.pcm.command
modules.pcm.routing
modules.pcm.snapshot
modules.hifi.command
modules.hifi.command_status
modules.hifi.zone
modules.hifi.controller
modules.metadata.now_playing
modules.metadata.cover_art
modules.metadata.playback
modules.metadata.history
modules.audio.state
```

---

## 6. Broker Validation and Efficiency

Every message receives envelope validation:

```text
- valid frame
- valid JSON
- version exists
- id exists
- source exists
- source is registered
- topic exists
- event exists
- correlationId exists
- payload exists and is an object
- message size below max
- source is allowed to publish topic
```

Commands that mutate state receive strict payload validation:

```text
modules.zone.command / set_zone_enabled
modules.pcm.command / route_start
modules.pcm.command / route_end
modules.pcm.command / set_zone_enabled
modules.hifi.command / set_zone_power_source
modules.shairport.command / stop_zone
```

Important state events should also be validated:

```text
modules.pcm.routing / owner_changed
modules.zone.config / zone_enabled_changed
modules.hifi.zone / zone_power_changed
modules.hifi.zone / zone_source_changed
modules.metadata.now_playing / metadata_track_changed
modules.metadata.cover_art / cover_art_updated
modules.metadata.history / play_history_updated
```

Fanout rule:

```text
Parse once.
Validate once.
Route many.
Do not re-parse/revalidate for every subscriber.
```

Idle target:

```text
core/events messages: 0/sec when no state changes are happening
no heartbeat spam
no per-second progress ticks
no SQLite writes in broker hot path
no PCM data on broker
no album art blobs on broker
```

---

## 7. Zone Runtime Model

HomePi always has 16 logical zones.

Disabled zones should consume no audio runtime resources.

Zone states:

```text
Disabled
  no Shairport service
  no ALSA capture handle
  no capture worker
  no drain
  no buffer
  not eligible for owner

EnabledIdle
  Shairport service may be running
  no PCM capture handle by default
  eligible for AirPlay play_begin

OpeningCapture
  PCM router is opening ALSA capture
  not eligible for owner until frames are ready

Buffering
  capture worker is running
  frames are being captured into ring buffer
  not owner yet

Owner
  DAC playback reads from this zone ring

ActiveNonOwner
  zone is active but not current owner
  mode can be Drain or Buffer depending policy

ClosingGrace
  play ended
  capture remains open briefly for quick reconnect

ClosedIdle
  enabled zone with capture closed
```

Initial implementation can use a simpler public state model:

```text
Disabled
EnabledIdle
Active
ClosingGrace
```

`OpeningCapture` and `Buffering` can remain internal PCM router states.

---

## 8. PCM Capture Modes

Recommended enum:

```cpp
enum class ZoneCaptureMode {
  Disabled,
  Drain,
  Buffer,
  Owner
};
```

Meaning:

```text
Disabled
  capture closed; no read; no drain; no worker

Drain
  capture open and active; read and discard frames

Buffer
  capture open and active; read frames into ring buffer

Owner
  capture open and active; DAC reads from this zone ring buffer
```

Disabled zones should never be represented as `Drain`.

---

## 9. Lazy Audio Loading Decision

PCM capture should be lazy.

Do not open captures merely because zones exist:

```text
bad startup model:
  open captures for zones 1–16
  start 16 capture workers
```

Recommended hybrid:

```text
Disabled zones:
  capture closed

Enabled idle zones:
  Shairport service running
  PCM capture closed

AirPlay active_begin:
  optional pre-open capture

AirPlay play_begin:
  open capture if not already open
  start capture worker
  wait for frames
  promote owner when ready

AirPlay play_end:
  keep capture open for short grace period
  close if no reconnect

UI disables zone:
  close capture immediately
```

Recommended defaults:

```json
{
  "pcmRouter": {
    "captureOpenPolicy": "on_active_begin_or_play_begin",
    "captureIdleCloseDelayMs": 5000,
    "captureDisableCloseTimeoutMs": 250,
    "ownerPromotionPolicy": "latest_ready_wins"
  }
}
```

---

## 10. ALSA Runtime Disable Decision

The PCM router should close HomePi's capture handle for disabled zones without restarting the service.

Can do without service restart:

```text
- close zone capture handle
- stop zone capture worker
- remove zone from active stack
- stop reading/draining that zone
```

Cannot do per-zone dynamically:

```text
- remove the ALSA loopback subdevice from the kernel
```

Disable flow:

```text
1. mark zone disabled
2. remove from active_stack
3. clear pending owner if matching zone
4. fallback owner if current owner was disabled
5. request capture worker stop
6. worker exits read loop
7. worker calls snd_pcm_drop(handle)
8. worker calls snd_pcm_close(handle)
9. clear ring buffer
10. publish zone_capture_closed
```

Do not close an ALSA handle from another thread while the capture worker may be blocked in `snd_pcm_readi`. Use `snd_pcm_wait` with a timeout or ALSA poll descriptors plus a wake event.

---

## 11. Pipe-Only Metadata Design

Each enabled Shairport zone writes metadata to a FIFO:

```text
/tmp/homepi-metadata-zone-1
/tmp/homepi-metadata-zone-2
...
/tmp/homepi-metadata-zone-16
```

Generated Shairport config for each enabled zone:

```conf
metadata =
{
  enabled = "yes";
  include_cover_art = "yes";
  cover_art_cache_directory = "";
  pipe_name = "/tmp/homepi-metadata-zone-7";
  progress_interval = 5.0;
};

mqtt =
{
  enabled = "no";
};
```

MQTT is not used for native HomePi metadata.

Metadata pipe ownership:

```text
disabled zone:
  Shairport stopped
  pipe not opened
  no drain
  no parse

enabled non-owner zone:
  pipe may be opened
  bytes are drained and discarded
  no parse
  no persist
  no event emit

owner zone:
  pipe opened
  bytes are parsed
  now-playing reducer is updated
  meaningful changes go to core/events
  realtime progress goes to audio-realtime.sock
```

### 11.1 Parser Requirements

The C++ parser must parse Shairport pipe items in the XML-style format:

```text
<item><type>...</type><code>...</code><length>...</length>
<data encoding="base64">...</data></item>
```

Supported `core` codes:

```text
core/minm -> title
core/asar -> artist
core/asal -> album
core/astm -> duration_ms
core/mper -> persistent_id / track_id candidate
core/asul -> stream_url
core/ascm -> comment
core/asgn -> genre
core/ascp -> composer
core/asdt -> file_kind
core/asdk -> song_data_kind
core/assn -> sort_title
```

Supported `ssnc` codes:

```text
ssnc/PICT -> cover art bytes
ssnc/prgr -> progress string start/current/end RTP frames
ssnc/phb0 -> first frame/time
ssnc/phbt -> playing frame/time
ssnc/snam -> client_name
ssnc/snua -> client_user_agent
ssnc/cmod -> client_model
ssnc/clip -> client_ip
ssnc/conn -> pending_client_ip
ssnc/disc -> disconnected_client_ip
ssnc/cdid -> client_device_id
ssnc/cmac -> client_mac
ssnc/svip -> server_ip
ssnc/svna -> shairport_service_name
ssnc/pvol -> airplay_volume
ssnc/pbeg -> play session begin
ssnc/pend -> play session end
ssnc/paus -> pause
ssnc/prsm -> resume
ssnc/abeg -> active state begin
ssnc/aend -> active state end
ssnc/mdst -> metadata bundle start
ssnc/mden -> metadata bundle end
ssnc/pcst -> picture start
ssnc/pcen -> picture end
ssnc/pfls -> flush
ssnc/stal -> metadata stalled warning
```

Unknown codes should be ignored unless debug tracing is enabled. Unknown-code logs must be rate-limited through `core/logging`.

### 11.2 Now Playing State

Recommended in-memory snapshot:

```cpp
struct NowPlayingSnapshot {
  int owner_zone_id = 0;

  std::string title;
  std::string artist;
  std::string album;
  std::string genre;
  std::string composer;
  std::string comment;
  std::string sort_title;
  std::string file_kind;
  std::string stream_url;

  std::string track_id;
  std::string persistent_id;

  std::string client_name;
  std::string client_model;
  std::string client_user_agent;
  std::string client_ip;
  std::string client_device_id;
  std::string client_mac;

  bool playing = false;
  int position_ms = 0;
  int duration_ms = 0;

  bool has_cover_art = false;
  std::string cover_art_id;
  std::string cover_art_path;

  std::string metadata_quality = "empty";
  std::string started_at;
  std::string updated_at;
};
```

Only the current PCM owner zone can update this state.

### 11.3 Metadata Coalescing

Do not write or emit after every field.

Recommended rules:

```text
track/client field updates:
  update memory immediately
  start/restart 150–250 ms coalesce timer
  emit one metadata_track_changed or metadata_client_updated event after timer fires
  persist one current now-playing row after timer fires

cover art:
  validate size and type
  hash bytes
  skip if same hash
  write content-addressed file
  emit one cover_art_updated event

progress:
  update memory immediately
  do not emit periodic ticks to core/events
  send latest-value corrections through audio-realtime.sock
  persist progress at most every 5 seconds and on stream end

history:
  insert once when stream finalizes
  trim to last 20 rows
```

---

## 12. Metadata Storage Schema

### 12.1 Current Now Playing

Single-row table:

```sql
CREATE TABLE IF NOT EXISTS audio_now_playing (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  owner_zone_id INTEGER NOT NULL DEFAULT 0,

  title TEXT NOT NULL DEFAULT '',
  artist TEXT NOT NULL DEFAULT '',
  album TEXT NOT NULL DEFAULT '',
  genre TEXT NOT NULL DEFAULT '',
  composer TEXT NOT NULL DEFAULT '',
  comment TEXT NOT NULL DEFAULT '',
  sort_title TEXT NOT NULL DEFAULT '',
  file_kind TEXT NOT NULL DEFAULT '',
  stream_url TEXT NOT NULL DEFAULT '',

  track_id TEXT NOT NULL DEFAULT '',
  persistent_id TEXT NOT NULL DEFAULT '',

  client_name TEXT NOT NULL DEFAULT '',
  client_model TEXT NOT NULL DEFAULT '',
  client_user_agent TEXT NOT NULL DEFAULT '',
  client_ip TEXT NOT NULL DEFAULT '',
  client_device_id TEXT NOT NULL DEFAULT '',
  client_mac TEXT NOT NULL DEFAULT '',

  playing INTEGER NOT NULL DEFAULT 0,
  position_ms INTEGER NOT NULL DEFAULT 0,
  duration_ms INTEGER NOT NULL DEFAULT 0,

  has_cover_art INTEGER NOT NULL DEFAULT 0,
  cover_art_id TEXT NOT NULL DEFAULT '',
  cover_art_path TEXT NOT NULL DEFAULT '',

  metadata_quality TEXT NOT NULL DEFAULT 'empty',
  started_at TEXT NOT NULL DEFAULT '',
  updated_at TEXT NOT NULL DEFAULT ''
);
```

### 12.2 Last 20 Play History

```sql
CREATE TABLE IF NOT EXISTS audio_play_history (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  stream_key TEXT NOT NULL,
  source TEXT NOT NULL DEFAULT 'airplay',

  zone_id INTEGER NOT NULL,
  title TEXT NOT NULL DEFAULT '',
  artist TEXT NOT NULL DEFAULT '',
  album TEXT NOT NULL DEFAULT '',
  track_id TEXT NOT NULL DEFAULT '',
  persistent_id TEXT NOT NULL DEFAULT '',

  client_name TEXT NOT NULL DEFAULT '',
  client_model TEXT NOT NULL DEFAULT '',
  client_ip TEXT NOT NULL DEFAULT '',

  duration_ms INTEGER NOT NULL DEFAULT 0,
  last_position_ms INTEGER NOT NULL DEFAULT 0,

  has_cover_art INTEGER NOT NULL DEFAULT 0,
  cover_art_id TEXT NOT NULL DEFAULT '',
  cover_art_path TEXT NOT NULL DEFAULT '',

  started_at TEXT NOT NULL DEFAULT '',
  ended_at TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL DEFAULT ''
);

CREATE INDEX IF NOT EXISTS idx_audio_play_history_ended_at
ON audio_play_history(ended_at DESC);
```

Trim rule:

```sql
DELETE FROM audio_play_history
WHERE id NOT IN (
  SELECT id FROM audio_play_history
  ORDER BY ended_at DESC, id DESC
  LIMIT 20
);
```

A stream is meaningful if it has at least one of:

```text
- title
- artist
- album
- track_id
- persistent_id
- cover art
- position_ms >= 10000
```

### 12.3 Track Duration Cache

Keep a duration cache keyed by `track_id` or `persistent_id`:

```sql
CREATE TABLE IF NOT EXISTS track_duration_cache (
  track_key TEXT PRIMARY KEY,
  duration_ms INTEGER NOT NULL,
  updated_at TEXT NOT NULL DEFAULT ''
);
```

### 12.4 Cover Art Cache

Do not store image bytes in SQLite.

Recommended paths:

```text
/opt/homepi/runtime/cache/metadata/artwork/current.jpg
/opt/homepi/runtime/cache/metadata/artwork/sha256-<hash>.jpg
```

Cleanup keeps:

```text
- current now-playing cover
- cover files referenced by last 20 history rows
```

Delete unreferenced cover files during maintenance.

---

## 13. Dedicated Audio Realtime Socket

`homepi-metadata` owns:

```text
/run/homepi/audio-realtime.sock
```

Implemented using `core/transport` primitives.

Preferred socket type:

```text
Unix domain SOCK_SEQPACKET
```

Fallback:

```text
Unix domain SOCK_STREAM + NDJSON
```

Socket contract:

```text
path: /run/homepi/audio-realtime.sock
owner service: homepi-metadata
implementation helpers: core/transport
client: backend audio bridge only
browser access: never direct
persistence: none
commands: not allowed
queue model: latest value wins
blocking policy: never block parser/reducer/storage/events/logging
```

### 13.1 Realtime Payload

Realtime socket sends latest-value snapshots, not durable events.

Example frame:

```json
{
  "type": "audio.realtime.snapshot",
  "schemaVersion": 1,
  "sequence": 1024,
  "monotonicMs": 28451239,
  "wallTime": "2026-06-20T20:10:03.000Z",
  "payload": {
    "ownerZoneId": 7,
    "trackId": "0x1234567890abcdef",
    "playing": true,
    "positionMs": 45000,
    "durationMs": 213000,
    "progressSource": "pipe:ssnc/prgr"
  }
}
```

Backend subscription request:

```json
{
  "method": "subscribeRealtime",
  "correlationId": "backend-audio-ui",
  "payload": {
    "stream": "audio.nowPlaying",
    "sendInitialSnapshot": true,
    "maxHz": 2
  }
}
```

### 13.2 Latest-Value Backpressure Rules

Realtime socket must never block metadata parsing.

```text
- all client sockets are non-blocking
- writes use MSG_DONTWAIT or equivalent core/transport helper
- each client has at most one pending realtime frame
- newer frame overwrites older pending frame
- no unbounded queues
- no disk writes in realtime publishing path
- no waiting on backend clients
- disconnect clients that remain blocked beyond configured limits
```

Generic helper may live in `core/transport`:

```cpp
namespace homepi::transport {
class LatestValuePublisher {
 public:
  void addClient(int fd);
  void removeClient(int fd);
  void publish(std::string frame);
  void handleWritable(int fd);
  void handleReadable(int fd);
};
}
```

Audio-domain wrapper stays in `homepi-metadata`:

```cpp
namespace homepi::metadata {
class AudioRealtimeServer {
 public:
  void publishSnapshot(const AudioRealtimeSnapshot& snapshot);
  void handleSubscribeRealtime(const SubscribeRealtimeRequest& request);
};
}
```

`core/transport::LatestValuePublisher` must remain payload-agnostic. It must not reference `AudioRealtimeSnapshot`, `positionMs`, `ownerZoneId`, or any audio-specific field.

---

## 14. Realtime UI Delivery

The browser never talks to native sockets.

Backend has two inputs:

```text
1. /run/homepi/events.sock
2. /run/homepi/audio-realtime.sock
```

Backend exposes UI-facing routes:

```text
GET /api/audio/now-playing
GET /api/audio/history?limit=20
GET /api/audio/now-playing/cover
GET /api/audio/history/:id/cover
GET /api/events/audio        # SSE recommended first
```

SSE/WebSocket output to UI combines event-bus state and realtime socket telemetry:

```json
{
  "type": "audio.trackChanged",
  "data": {
    "title": "Song Title",
    "artist": "Artist Name",
    "album": "Album Name",
    "coverArtUrl": "/api/audio/now-playing/cover?v=sha256-91d4"
  }
}
```

```json
{
  "type": "audio.realtime",
  "data": {
    "positionMs": 43000,
    "durationMs": 213000,
    "playing": true,
    "serverMonotonicMs": 28450000
  }
}
```

The UI animates progress locally between realtime corrections:

```ts
function getDisplayedPositionMs(snapshot: AudioRealtimeSnapshot) {
  if (!snapshot.playing) return snapshot.positionMs;

  const elapsed = performance.now() - snapshot.receivedAtMs;
  return Math.min(snapshot.positionMs + elapsed, snapshot.durationMs);
}
```

This makes the progress bar smooth without pushing periodic ticks through `core/events`.

---

## 15. `epoll` Event Loop Design for `homepi-metadata`

`homepi-metadata` should use one C++ epoll-based event loop.

This is not busy polling. The daemon sleeps in the kernel until a watched file descriptor is ready.

The event loop watches:

```text
- enabled Shairport metadata pipe fds
- core/events client socket fd
- /run/homepi/audio-realtime.sock listener fd
- connected backend realtime client fds
- timerfd for metadata coalescing
- timerfd for progress persistence
- timerfd for maintenance/artwork cleanup
- eventfd for shutdown/reload wakeups
```

### 15.1 Pipe FD Behavior

Open enabled-zone pipes as non-blocking:

```cpp
int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
```

Register with epoll:

```cpp
epoll_event ev{};
ev.events = EPOLLIN;
ev.data.fd = pipe_fd;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipe_fd, &ev);
```

When readable:

```text
if zone == ownerZoneId:
  read until EAGAIN
  feed parser
  update reducer

else:
  read until EAGAIN
  discard bytes
```

### 15.2 Core Events FD Behavior

`homepi-metadata` subscribes to:

```text
modules.pcm.routing
modules.zone.config
core.service
```

Owner-change handling:

```text
1. receive modules.pcm.routing / owner_changed
2. finalize previous stream if needed
3. update ownerZoneId
4. reset owner parser
5. parse new owner pipe from this point forward
6. emit metadata_owner_changed if user-visible
```

### 15.3 Realtime Socket Behavior

Create listener:

```cpp
int listen_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0);
bind(listen_fd, "/run/homepi/audio-realtime.sock");
listen(listen_fd, SOMAXCONN);
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, ...);
```

When listener is readable:

```cpp
int client_fd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
```

Register backend client fd with epoll.

Client fd watches:

```text
EPOLLIN  -> subscription/control messages from backend
EPOLLOUT -> enabled only when pending frame exists
EPOLLHUP / EPOLLERR -> disconnect cleanup
```

### 15.4 Timer FD Behavior

Use `timerfd` instead of fixed timeout loops.

Timers:

```text
metadata_flush_timerfd:
  one-shot 150–250 ms after track/client field changes

progress_persist_timerfd:
  every 5 seconds only while playing

maintenance_timerfd:
  every 30–60 seconds for pipe reopen/artwork cleanup/history cleanup
```

### 15.5 Event FD Behavior

Use `eventfd` for shutdown/reload.

```text
shutdown requested:
  write eventfd
  epoll wakes
  daemon exits cleanly
```

### 15.6 Event Loop Skeleton

```cpp
while (running) {
  epoll_event events[64];
  int n = epoll_wait(epoll_fd_, events, 64, -1);

  if (n < 0) {
    if (errno == EINTR) continue;
    logError("metadata.epoll.failed");
    break;
  }

  for (int i = 0; i < n; ++i) {
    const int fd = events[i].data.fd;
    const uint32_t flags = events[i].events;

    if (fd == shutdown_fd_) {
      handleShutdown();
    } else if (fd == core_events_fd_) {
      handleCoreEventsReadable();
    } else if (fd == realtime_listen_fd_) {
      acceptRealtimeClients();
    } else if (isRealtimeClient(fd)) {
      handleRealtimeClient(fd, flags);
    } else if (fd == metadata_flush_timer_fd_) {
      flushCoalescedMetadata();
    } else if (fd == progress_persist_timer_fd_) {
      persistProgressIfDue();
    } else if (fd == maintenance_timer_fd_) {
      runMaintenance();
    } else if (isPipeFd(fd)) {
      handlePipeReadable(fd);
    }
  }
}
```

### 15.7 Level Triggered First

Use level-triggered epoll first.

```text
- simpler and safer
- non-blocking fds still required
- read until EAGAIN
- move to EPOLLET only if profiling proves it matters
```

On Raspberry Pi with 16 zones, level-triggered epoll is sufficient.

---

## 16. Metadata Events Through `core/events`

Only meaningful state changes go through `core/events`.

### 16.1 Metadata Owner Changed

```json
{
  "version": 1,
  "id": "evt-metadata-owner-z7-001",
  "source": "homepi-metadata",
  "topic": "modules.metadata.now_playing",
  "event": "metadata_owner_changed",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T00:00:08.146Z",
  "payload": {
    "ownerZoneId": 7,
    "previousOwnerZoneId": 3
  }
}
```

### 16.2 Track Changed

```json
{
  "version": 1,
  "id": "evt-metadata-track-z7-001",
  "source": "homepi-metadata",
  "topic": "modules.metadata.now_playing",
  "event": "metadata_track_changed",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T20:10:00.250Z",
  "payload": {
    "ownerZoneId": 7,
    "trackId": "0x1234567890abcdef",
    "title": "Song Title",
    "artist": "Artist Name",
    "album": "Album Name",
    "clientName": "Nick’s iPhone",
    "clientModel": "iPhone",
    "clientIp": "10.3.3.42",
    "durationMs": 213000,
    "hasCoverArt": false,
    "metadataQuality": "partial"
  }
}
```

### 16.3 Cover Art Updated

```json
{
  "version": 1,
  "id": "evt-metadata-cover-z7-001",
  "source": "homepi-metadata",
  "topic": "modules.metadata.cover_art",
  "event": "cover_art_updated",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T20:10:00.400Z",
  "payload": {
    "ownerZoneId": 7,
    "trackId": "0x1234567890abcdef",
    "coverArtId": "sha256-91d4...",
    "coverArtUrl": "/api/audio/now-playing/cover?v=sha256-91d4",
    "hasCoverArt": true
  }
}
```

### 16.4 Playback State Changed

Use this for pause/resume/stop transitions, not periodic progress.

```json
{
  "version": 1,
  "id": "evt-metadata-playback-z7-paused-001",
  "source": "homepi-metadata",
  "topic": "modules.metadata.playback",
  "event": "playback_state_changed",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T20:11:12.000Z",
  "payload": {
    "ownerZoneId": 7,
    "playing": false,
    "positionMs": 87000,
    "durationMs": 213000
  }
}
```

### 16.5 History Updated

```json
{
  "version": 1,
  "id": "evt-metadata-history-updated-001",
  "source": "homepi-metadata",
  "topic": "modules.metadata.history",
  "event": "play_history_updated",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T20:14:02.000Z",
  "payload": {
    "limit": 20,
    "latestHistoryId": 184,
    "latest": {
      "zoneId": 7,
      "title": "Song Title",
      "artist": "Artist Name",
      "album": "Album Name",
      "clientName": "Nick’s iPhone",
      "hasCoverArt": true,
      "coverArtUrl": "/api/audio/history/184/cover",
      "startedAt": "2026-06-20T20:10:00.250Z",
      "endedAt": "2026-06-20T20:14:02.000Z"
    }
  }
}
```

---

## 17. Audio Lifecycle Message Flows

### 17.1 Service Startup

PCM router registers:

```json
{
  "version": 1,
  "id": "evt-pcm-register-001",
  "source": "homepi-pcm-router",
  "topic": "core.broker",
  "event": "register",
  "correlationId": "startup",
  "timestamp": "2026-06-20T00:00:00.000Z",
  "payload": {
    "service": "homepi-pcm-router",
    "capabilities": [
      "modules.pcm.command.route_start",
      "modules.pcm.command.route_end",
      "modules.pcm.command.set_zone_enabled",
      "modules.pcm.command.set_routing"
    ],
    "publishes": [
      "modules.pcm.routing",
      "modules.pcm.snapshot",
      "core.service"
    ]
  }
}
```

Metadata registers:

```json
{
  "version": 1,
  "id": "evt-metadata-register-001",
  "source": "homepi-metadata",
  "topic": "core.broker",
  "event": "register",
  "correlationId": "startup",
  "timestamp": "2026-06-20T00:00:00.010Z",
  "payload": {
    "service": "homepi-metadata",
    "capabilities": [
      "metadata.pipe.owner_zone_parse",
      "metadata.audio_realtime_socket",
      "metadata.play_history.last20"
    ],
    "publishes": [
      "modules.metadata.now_playing",
      "modules.metadata.cover_art",
      "modules.metadata.playback",
      "modules.metadata.history",
      "core.service"
    ],
    "sockets": [
      "/run/homepi/audio-realtime.sock"
    ]
  }
}
```

### 17.2 AirPlay Starts on Zone 3

Shairport active begin:

```json
{
  "version": 1,
  "id": "evt-shairport-z3-active-begin-001",
  "source": "homepi-shairport-supervisor",
  "topic": "modules.shairport.session",
  "event": "active_begin",
  "correlationId": "airplay-session-z3-001",
  "timestamp": "2026-06-20T00:00:01.000Z",
  "payload": {
    "zoneId": 3,
    "sessionId": "z3-airplay-001",
    "trigger": "run_this_before_entering_active_state"
  }
}
```

Orchestrator prewarms PCM capture:

```json
{
  "version": 1,
  "id": "cmd-pcm-prewarm-z3-001",
  "source": "homepi-audio-orchestrator",
  "topic": "modules.pcm.command",
  "event": "prewarm_capture",
  "correlationId": "airplay-session-z3-001",
  "timestamp": "2026-06-20T00:00:01.002Z",
  "payload": {
    "zoneId": 3,
    "reason": "shairport.active_begin",
    "openCapture": true
  }
}
```

Shairport play begin:

```json
{
  "version": 1,
  "id": "evt-shairport-z3-play-begin-001",
  "source": "homepi-shairport-supervisor",
  "topic": "modules.shairport.session",
  "event": "play_begin",
  "correlationId": "airplay-session-z3-001",
  "timestamp": "2026-06-20T00:00:01.100Z",
  "payload": {
    "zoneId": 3,
    "state": "playing",
    "sessionId": "z3-airplay-001",
    "sourceNumber": 7,
    "trigger": "run_this_before_play_begins"
  }
}
```

Orchestrator routes PCM:

```json
{
  "version": 1,
  "id": "cmd-pcm-route-start-z3-001",
  "source": "homepi-audio-orchestrator",
  "topic": "modules.pcm.command",
  "event": "route_start",
  "correlationId": "airplay-session-z3-001",
  "timestamp": "2026-06-20T00:00:01.102Z",
  "payload": {
    "zoneId": 3,
    "reason": "shairport.play_begin",
    "ownerPolicy": "latest_ready_wins",
    "openCapture": true,
    "replyRequested": true
  }
}
```

Orchestrator powers Hi-Fi2 zone/source:

```json
{
  "version": 1,
  "id": "cmd-hifi-z3-power-source-001",
  "source": "homepi-audio-orchestrator",
  "topic": "modules.hifi.command",
  "event": "set_zone_power_source",
  "correlationId": "airplay-session-z3-001",
  "timestamp": "2026-06-20T00:00:01.103Z",
  "payload": {
    "zoneNumber": 3,
    "power": true,
    "sourceNumber": 7,
    "priority": "realtime_control",
    "replyRequested": true
  }
}
```

PCM owner changes when frames are ready:

```json
{
  "version": 1,
  "id": "evt-pcm-owner-changed-z3-001",
  "source": "homepi-pcm-router",
  "topic": "modules.pcm.routing",
  "event": "owner_changed",
  "correlationId": "airplay-session-z3-001",
  "timestamp": "2026-06-20T00:00:01.130Z",
  "payload": {
    "ownerZoneId": 3,
    "previousOwnerZoneId": 0,
    "pendingOwnerZoneId": 0,
    "activeStack": [3],
    "reason": "zone_ready"
  }
}
```

Metadata follows owner zone 3:

```json
{
  "version": 1,
  "id": "evt-metadata-owner-z3-001",
  "source": "homepi-metadata",
  "topic": "modules.metadata.now_playing",
  "event": "metadata_owner_changed",
  "correlationId": "airplay-session-z3-001",
  "timestamp": "2026-06-20T00:00:01.131Z",
  "payload": {
    "ownerZoneId": 3,
    "previousOwnerZoneId": 0
  }
}
```

### 17.3 Another AirPlay Zone Is Added

Assumptions:

```text
zone 3 is current owner
zone 7 starts playing
policy is latest_ready_wins
```

Zone 7 play begins:

```json
{
  "version": 1,
  "id": "evt-shairport-z7-play-begin-001",
  "source": "homepi-shairport-supervisor",
  "topic": "modules.shairport.session",
  "event": "play_begin",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T00:00:08.100Z",
  "payload": {
    "zoneId": 7,
    "state": "playing",
    "sessionId": "z7-airplay-001",
    "sourceNumber": 7
  }
}
```

PCM router marks zone 7 pending:

```json
{
  "version": 1,
  "id": "evt-pcm-owner-pending-z7-001",
  "source": "homepi-pcm-router",
  "topic": "modules.pcm.routing",
  "event": "owner_pending",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T00:00:08.104Z",
  "payload": {
    "ownerZoneId": 3,
    "pendingOwnerZoneId": 7,
    "activeStack": [3, 7],
    "reason": "waiting_for_zone_buffer"
  }
}
```

PCM router promotes zone 7 only after frames are ready:

```json
{
  "version": 1,
  "id": "evt-pcm-owner-changed-z7-001",
  "source": "homepi-pcm-router",
  "topic": "modules.pcm.routing",
  "event": "owner_changed",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T00:00:08.145Z",
  "payload": {
    "ownerZoneId": 7,
    "previousOwnerZoneId": 3,
    "pendingOwnerZoneId": 0,
    "activeStack": [7, 3],
    "reason": "pending_owner_ready"
  }
}
```

Metadata switches parser ownership to zone 7:

```json
{
  "version": 1,
  "id": "evt-metadata-owner-z7-001",
  "source": "homepi-metadata",
  "topic": "modules.metadata.now_playing",
  "event": "metadata_owner_changed",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T00:00:08.146Z",
  "payload": {
    "ownerZoneId": 7,
    "previousOwnerZoneId": 3
  }
}
```

### 17.4 Zone Stops and Owner Falls Back

Shairport zone 7 ends:

```json
{
  "version": 1,
  "id": "evt-shairport-z7-play-end-001",
  "source": "homepi-shairport-supervisor",
  "topic": "modules.shairport.session",
  "event": "play_end",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T00:01:30.000Z",
  "payload": {
    "zoneId": 7,
    "state": "stopping",
    "sessionId": "z7-airplay-001"
  }
}
```

PCM owner falls back:

```json
{
  "version": 1,
  "id": "evt-pcm-owner-changed-z3-002",
  "source": "homepi-pcm-router",
  "topic": "modules.pcm.routing",
  "event": "owner_changed",
  "correlationId": "airplay-session-z7-001",
  "timestamp": "2026-06-20T00:01:30.004Z",
  "payload": {
    "ownerZoneId": 3,
    "previousOwnerZoneId": 7,
    "activeStack": [3],
    "reason": "route_end_fallback"
  }
}
```

---

## 18. Zone Enable/Disable Message Flows

### 18.1 UI Disables an Idle Zone

Frontend command:

```json
{
  "version": 1,
  "id": "cmd-zone-disable-z7-001",
  "source": "homepi-backend",
  "topic": "modules.zone.command",
  "event": "set_zone_enabled",
  "correlationId": "ui-zone-7-disabled",
  "timestamp": "2026-06-20T00:02:00.000Z",
  "payload": {
    "zoneId": 7,
    "enabled": false,
    "applyToController": true
  }
}
```

Orchestrator publishes config change:

```json
{
  "version": 1,
  "id": "evt-zone-z7-disabled-001",
  "source": "homepi-audio-orchestrator",
  "topic": "modules.zone.config",
  "event": "zone_enabled_changed",
  "correlationId": "ui-zone-7-disabled",
  "timestamp": "2026-06-20T00:02:00.005Z",
  "payload": {
    "zoneId": 7,
    "enabled": false,
    "enabledZones": [1, 2, 3, 4, 5, 6, 8],
    "disabledZones": [7, 9, 10, 11, 12, 13, 14, 15, 16]
  }
}
```

Stop Shairport zone service:

```json
{
  "version": 1,
  "id": "cmd-shairport-stop-z7-001",
  "source": "homepi-audio-orchestrator",
  "topic": "modules.shairport.command",
  "event": "stop_zone",
  "correlationId": "ui-zone-7-disabled",
  "timestamp": "2026-06-20T00:02:00.006Z",
  "payload": {
    "zoneId": 7,
    "reason": "zone_disabled"
  }
}
```

PCM router marks zone disabled:

```json
{
  "version": 1,
  "id": "cmd-pcm-disable-z7-001",
  "source": "homepi-audio-orchestrator",
  "topic": "modules.pcm.command",
  "event": "set_zone_enabled",
  "correlationId": "ui-zone-7-disabled",
  "timestamp": "2026-06-20T00:02:00.007Z",
  "payload": {
    "zoneId": 7,
    "enabled": false,
    "closeCapture": true,
    "removeFromActiveStack": true
  }
}
```

### 18.2 UI Disables the Current Owner

If active stack is `[7, 3]` and zone 7 is disabled, PCM router promotes fallback zone 3:

```json
{
  "version": 1,
  "id": "evt-pcm-owner-fallback-z3-001",
  "source": "homepi-pcm-router",
  "topic": "modules.pcm.routing",
  "event": "owner_changed",
  "correlationId": "ui-zone-7-disabled",
  "timestamp": "2026-06-20T00:03:00.012Z",
  "payload": {
    "ownerZoneId": 3,
    "previousOwnerZoneId": 7,
    "disabledZoneId": 7,
    "activeStack": [3],
    "reason": "owner_disabled"
  }
}
```

PCM closes zone 7 immediately:

```json
{
  "version": 1,
  "id": "evt-pcm-z7-capture-closed-disable-001",
  "source": "homepi-pcm-router",
  "topic": "modules.pcm.routing",
  "event": "zone_capture_closed",
  "correlationId": "ui-zone-7-disabled",
  "timestamp": "2026-06-20T00:03:00.013Z",
  "payload": {
    "zoneId": 7,
    "captureOpen": false,
    "reason": "zone_disabled"
  }
}
```

---

## 19. Runtime PCM Router Snapshot

Full snapshots should be published on request/startup, not continuously.

```json
{
  "version": 1,
  "id": "evt-pcm-snapshot-001",
  "source": "homepi-pcm-router",
  "topic": "modules.pcm.snapshot",
  "event": "pcm_router_snapshot",
  "correlationId": "snapshot",
  "timestamp": "2026-06-20T00:05:00.000Z",
  "payload": {
    "zoneCount": 16,
    "enabledZones": [1, 2, 3, 4, 5, 6, 8],
    "disabledZones": [7, 9, 10, 11, 12, 13, 14, 15, 16],
    "ownerZoneId": 3,
    "pendingOwnerZoneId": 0,
    "activeStack": [3],
    "capture": {
      "openZones": [3],
      "closingGraceZones": [],
      "disabledZonesClosed": true
    },
    "dac": {
      "state": "open",
      "deviceId": "primary_audio"
    },
    "policy": {
      "captureOpenPolicy": "on_active_begin_or_play_begin",
      "captureIdleCloseDelayMs": 5000,
      "ownerPromotionPolicy": "latest_ready_wins"
    }
  }
}
```

---

## 20. Latency Expectations

From tapping Play in AirPlay to hearing sound from speakers:

```text
Best case / warm path:         ~1.0–2.0 seconds
Cold-ish first start:          ~2.0–4.0 seconds
Worst case / reconnect churn:  ~4.0+ seconds
```

HomePi-added latency target:

```text
broker routing:                <1–5 ms
orchestrator decision:          <1–5 ms
lazy capture open/configure:    ~10–100 ms
wait for safe PCM frames:       ~10–50 ms
DAC open/configure if cold:     ~10–150 ms
Hi-Fi2 POWER + SOURCE queue:    100+ ms minimum due command spacing
```

Instrumentation points:

```text
T0  shairport hook received active_begin
T1  shairport hook received play_begin
T2  broker accepted event
T3  orchestrator sent route_start
T4  pcm-router opened capture
T5  first PCM frames captured
T6  owner promoted
T7  first DAC write
T8  hifi POWER command written
T9  hifi SOURCE command written
T10 hifi zone power/source confirmed
```

Target:

```text
T7 - T1 < 150 ms
```

---

## 21. Logging Requirements

Every native service must log through `core/logging`.

Example native log call:

```cpp
core::log::write(
  core::log::Level::INFO,
  core::log::Event::io_started,
  "homepi-pcm-router",
  "Zone capture opened",
  R"({"zoneId":7,"alsaDevice":"hw:HomePiZonesA,0,6","elapsedMs":18})",
  "airplay-session-z7-001"
);
```

Resulting log shape:

```json
{
  "ts": "2026-06-20T00:00:01.015Z",
  "service": "homepi-pcm-router",
  "level": "INFO",
  "event": "io_started",
  "correlationId": "airplay-session-z7-001",
  "message": "Zone capture opened",
  "data": {
    "zoneId": 7,
    "alsaDevice": "hw:HomePiZonesA,0,6",
    "elapsedMs": 18
  }
}
```

Do log:

```text
- startup/shutdown
- config loaded/config error
- service registered/disconnected
- capture opened/closed
- DAC opened/closed/unavailable
- owner promoted/fallback/cleared
- Hi-Fi2 command queued/write failed
- serial disconnected/reconnected
- validation errors
- slow realtime client disconnected
- metadata pipe parse errors, rate-limited
- cover art rejected/write failed
```

Do not log:

```text
- every audio period
- every PCM frame/buffer read
- every broker fanout
- every metadata progress correction
- heartbeat spam
```

---

## 22. Recommended `homepi-metadata` C++ Layout

```text
services/homepi-metadata/
  include/homepi/metadata/
    metadata-daemon.hpp
    pipe-watcher.hpp
    metadata-parser.hpp
    metadata-reducer.hpp
    realtime-server.hpp
    progress-clock.hpp
    artwork-cache.hpp
    metadata-repository.hpp
    events-client.hpp
    service-config.hpp

  src/
    main.cpp
    metadata-daemon.cpp
    pipe-watcher.cpp
    metadata-parser.cpp
    metadata-reducer.cpp
    realtime-server.cpp
    progress-clock.cpp
    artwork-cache.cpp
    metadata-repository.cpp
    events-client.cpp
    service-config.cpp

  tests/
    test-metadata-parser.cpp
    test-progress-parser.cpp
    test-reducer.cpp
    test-artwork-cache.cpp
    test-realtime-backpressure.cpp
```

Dependency direction:

```text
metadata-daemon
  -> pipe-watcher
  -> metadata-parser
  -> metadata-reducer
  -> realtime-server
  -> metadata-repository
  -> events-client
  -> artwork-cache
  -> core/logging
  -> core/storage
  -> core/events
  -> core/transport
```

---

## 23. Implementation Plan

### Phase 1: Broker Foundation

```text
1. Add /run/homepi/events.sock broker service.
2. Move socket framing into core/transport.
3. Add register/subscribe/publish/request-reply message types.
4. Add envelope validation.
5. Add source/topic permission checks.
6. Add per-client output queues.
```

### Phase 2: Shairport Hook Simplification

```text
1. Replace Bash orchestration hook with tiny helper.
2. Helper publishes Shairport session events only.
3. Remove direct nc calls to pcm-router/hifi-serial from hook.
4. Remove Python parsing from hook.
```

### Phase 3: Audio Orchestrator

```text
1. Add homepi-audio-orchestrator.
2. Subscribe to Shairport session events.
3. Maintain active session stack.
4. Command PCM router.
5. Command Hi-Fi serial service.
6. Publish modules.audio.state.
```

### Phase 4: PCM Router Runtime Enable Mask

```text
1. Add enabled zone mask.
2. Add ZoneCaptureMode::Disabled.
3. Remove disabled zones from active_stack.
4. Ignore route_start for disabled zones.
5. Publish zone_disabled event.
```

### Phase 5: Lazy Per-Zone Capture

```text
1. Replace global capture handle vector with per-zone capture lifecycle.
2. Add enable_zone_capture / disable_zone_capture.
3. Add prewarm_capture command.
4. Open capture on active_begin or play_begin.
5. Close capture after idle grace.
6. Close immediately on UI disable.
```

### Phase 6: Typed Hi-Fi Commands

```text
1. Add typed command handlers.
2. Keep raw sendCommand for admin/debug only.
3. Ensure command queue owns protocol ordering and 50 ms spacing.
```

### Phase 7: Pipe-Only Metadata Rebuild

```text
1. Keep homepi-metadata as native C++20 daemon.
2. Remove MQTT from native metadata path.
3. Disable Shairport MQTT in generated configs.
4. Keep Shairport metadata pipe enabled with cover art.
5. Replace direct PCM socket subscription with core/events subscription.
6. Open metadata pipes for enabled zones only.
7. Drain enabled non-owner pipes, parse only owner pipe.
8. Expand parser codes.
9. Add audio_now_playing table.
10. Add audio_play_history last-20 table.
11. Add content-addressed artwork cache.
12. Add /run/homepi/audio-realtime.sock owned by homepi-metadata.
13. Implement realtime socket using core/transport helpers.
14. Add latest-value backpressure behavior.
15. Keep progress off core/events.
16. Use core/logging only.
```

---

## 24. Acceptance Criteria

### Idle Efficiency

```text
When no zones are active:
  no PCM capture workers running
  no ALSA capture handles open
  no broker message traffic
  no service heartbeat spam
  no disabled-zone drain loops
```

### Zone Disable Without Restart

```text
Given zone 7 is enabled
When UI disables zone 7
Then homepi-shairport@7 stops
And PCM router closes zone 7 capture handle
And zone 7 is removed from active stack
And no PCM router service restart occurs
```

### Active Owner Disable

```text
Given ownerZoneId = 7
And activeStack = [7, 3]
When zone 7 is disabled
Then PCM owner changes to 3
And zone 7 capture closes immediately
And Hi-Fi2 zone 7 powers off/disables if configured
```

### Pipe-Only Metadata

```text
Given Shairport emits metadata to /tmp/homepi-metadata-zone-N
Then homepi-metadata parses metadata from the pipe only
And native metadata does not require MQTT
And non-owner enabled pipes are drained but not parsed
And disabled-zone pipes are not opened or drained
And cover-art bytes are cached on disk, not sent through core/events
And last 20 meaningful streams are persisted through core/storage
And all metadata logs use core/logging
```

### Event Bus Protection

```text
Given playback is active
Then periodic playback position corrections are not published through core/events
And progress is available over /run/homepi/audio-realtime.sock
And core/events only receives meaningful state changes
```

### Realtime Backpressure

```text
Given backend stops reading realtime frames
Then homepi-metadata does not block
And only the latest pending frame is retained
And stale pending frames are overwritten
And the slow client is disconnected when limits are exceeded
```

### Transport Ownership Boundary

```text
Given /run/homepi/audio-realtime.sock exists
Then it is owned by homepi-metadata
And implemented using core/transport helpers
And core/transport does not contain audio-specific payloads or endpoint lifecycle logic
```

### Core Logging Only

```text
Given any native service emits a log
Then the log is written through core/logging
And the log uses the shared core log schema
And the service does not implement its own logging schema or log file path
```

### No Audio Through Broker

```text
Broker must never carry PCM audio frames.
```

---

## 25. Final Architecture Summary

```text
/run/homepi/events.sock
  shared control-plane event bus

/run/homepi/audio-realtime.sock
  latest-value playback telemetry endpoint
  owned by homepi-metadata
  implemented with core/transport

core/transport
  socket/framing/non-blocking IO/realtime publisher helpers

core/events
  broker, validation, routing, subscriptions

core/storage
  SQLite connection, migrations, repositories

core/logging
  structured native logs, rate limiting, watchdog helpers

homepi-shairport-supervisor
  process/config lifecycle + hook event publisher

homepi-audio-orchestrator
  domain state machine for AirPlay/PCM/Hi-Fi2

homepi-pcm-router
  lazy ALSA capture + DAC owner selection

homepi-hifi-serial
  typed Hi-Fi2 command execution + confirmed state

homepi-metadata
  pipe-only owner-zone now-playing reducer
  epoll daemon
  realtime socket owner
  cover art cache
  last-20 play history
```

The result is a HomePi architecture where disabled zones have zero audio runtime cost, enabled idle zones are lightweight, AirPlay session changes are explicit events, PCM capture is opened only when a zone needs it, metadata is reduced from Shairport pipes into one global now-playing state, progress is delivered to the UI without loading the event bus, and every native service uses the shared core services.

---

## 26. Implementation Progress

This section tracks rollout status against §23. The canonical recovery spec for work lost in the June 2026 Pi reflash is [`docs/latest-agent-chat.md`](latest-agent-chat.md) (Cursor session ending ~2026-06-22).

### 26.1 Git baseline vs. target

| Item | State |
|------|--------|
| **GitHub `main` HEAD** | `d6cdf4d` — Phase 1 broker foundation only |
| **Prior session achievement** | Phases 1–7 complete on Pi, orchestrator broker migration, seamless multi-zone handoff, now-playing UI parity |
| **Lost in reflash** | All code after `d6cdf4d` (never committed) |
| **Recovery approach** | Rebuild in phase order using `latest-agent-chat.md` as the functional spec |

### 26.2 Operational fix — primary audio save (completed 2026-06-22)

**Symptom:** Saving Primary Audio Output in Settings failed; USB devices disappeared; `pcm-router` and `shairport` stayed stopped; Sources tab unreachable.

**Root cause:** `setAssignments` blocked synchronously on `post-assignment-hook.sh` during ALSA USB reload. Concurrent udev hotplug + SQLite writes from `homepi-hifi-serial` restart caused `homepi-usb-devices` to abort (`database is locked`). The hook stopped audio consumers but did not finish restarting them.

**Fix (in repo, post-`d6cdf4d`):**

| Change | File |
|--------|------|
| Detached async hook (no socket timeout) | `services/homepi-usb-devices/src/post-assignment-hook.cpp` |
| Skip hotplug rescans while hook lock held | `services/homepi-usb-devices/src/main.cpp` |
| `EXIT` trap restarts pcm-router/shairport + rebinds USB | `services/homepi-usb-devices/scripts/post-assignment-hook.sh` |
| WAL + longer SQLite busy timeout | `core/storage/cpp/src/database-connection.cpp` |
| Graceful capability load on lock | `services/homepi-usb-devices/src/audio-profile-service.cpp` |

**Verify after fresh install:** Save all three USB roles in **Audio → Settings**; all devices remain listed; `systemctl is-active homepi-pcm-router homepi-shairport-supervisor` stays `active`; `grep HomePiPrimary /proc/asound/cards` succeeds.

### 26.3 Phase status (recovery target)

Status reflects the **end of `latest-agent-chat.md`** (user confirmed seamless zone add/drop and AirPlay icon correctness).

| Phase | §23 scope | Prior session | In git now | Recovery |
|-------|-----------|---------------|------------|----------|
| **1** | `core/events` broker + transport helpers | Done | Partial (`d6cdf4d`) | Complete request-reply, bounded queues, retire duplicate SSE bridges |
| **2** | Thin Shairport hook → broker events | Done | **Recovered** | Verify on hardware |
| **3** | `homepi-audio-orchestrator` | Done | **Recovered** | Verify on hardware |
| **4** | PCM enabled-zone mask + broker commands | Done | **Recovered** | Verify on hardware |
| **5** | Lazy per-zone capture | Done | **Recovered** | Verify on hardware |
| **6** | Typed Hi-Fi commands | Done | **Recovered** | Verify on hardware |
| **7** | Pipe-only metadata + `audio-realtime.sock` | Done | Done (`0ad5498`) | — |
| **8** | Broker-only migration (socket removal) | Orchestrator on broker only | Done | — |
| **9** | UI parity + handoff polish | Done (owner promotion, client pill, metadata SSE) | Partial | Rebuild with 2–7 |

### 26.4 End-of-session behavior checklist

Use this to confirm recovery matches the prior session:

1. **AirPlay single zone** — power, source switch, PCM route, now-playing metadata, progress bar, cover art
2. **Multi-zone add** — no audible gap; AirPlay icon follows DAC owner (`ownerZoneId` / `latest_ready_wins`)
3. **Multi-zone drop** — remaining zone continues; Hi-Fi follows owner
4. **Zone disable from UI** — Shairport stops; PCM capture closes without router restart
5. **Volume from AirPlay client** — Hi-Fi zone volume updates
6. **Now-playing dropdown** — client name pill under album; artwork aligned with title/artist/album
7. **Play history** — last 20 streams in `audio_play_history` after track change
8. **Progress** — smooth updates via `audio-realtime.sock` (not event-bus spam)
9. **USB assignment save** — no service crash (§26.2 fix)

### 26.5 Recovery build order

```text
1. Phase 2+3 — thin hook + homepi-audio-orchestrator (dual-stack sockets OK initially)
2. Phase 4 — PCM enabled mask + broker subscription
3. Phase 5 — lazy per-zone capture + prewarm_capture
4. Phase 6 — typed Hi-Fi commands (demote raw sendCommand)
5. Phase 7 — metadata rebuild (no MQTT, audio-realtime.sock, play history)
6. Phase 8 — orchestrator + backend broker-only; retire legacy event sockets
7. Phase 9 — UI/handoff fixes (owner promotion, client_name persistence, Shairport active_state_timeout)
8. Commit + push each stable milestone
```

### 26.6 Known gaps to close after recovery

- `core/events`: request-reply, full envelope validation, per-client output queues (§23 Phase 1 items 3–6)
- `core/logging`: native services still mix `std::cout` / ad-hoc loggers
- Consolidate `metadata.db` → `homepi.sqlite` (optional, noted in prior session)
- Move remaining hook Python (`send_hifi_typed`) into `homepi-shairport-hook` C++ binary

### 26.7 Commit discipline

The prior session lost all work after `d6cdf4d` because changes were not pushed before power loss. **Commit and push after each phase milestone passes hardware verification.**

