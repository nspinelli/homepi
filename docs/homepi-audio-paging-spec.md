# HomePi Home Audio Paging Feature Specification

## Document Purpose

This document defines the production-ready design for the **Paging** feature inside the HomePi Home Audio module. It is intended to be used directly in Cursor as an implementation guide.

The paging feature allows HomePi to accept text through an API, convert that text to speech using a local TTS engine, play the generated audio through a dedicated paging DAC, and activate the HAI/Leviton Hi-Fi2 system paging mode so the announcement is broadcast through the house.

Primary use case: **iPhone Shortcuts automations** (e.g. doorbell push → API call with alert text).

This feature must follow the broader HomePi architecture:

- Event-driven design.
- No polling for state that can be event-based.
- All persistence through `core/database`.
- All logs through `core/logging`.
- All service-to-service communication through `core/events` and `core/broker`.
- USB device identity and ALSA mapping through `core/usb`.
- HiFi2 serial commands routed through the existing HiFi2 controller path.
- Lightweight and reliable on Raspberry Pi hardware.
- Efficient SD card usage.
- Low-latency paging from API call to audible speech.
- Minimal CPU and ALSA resource use when paging is idle.

---

## Decision Log

Locked product and engineering decisions (updated as the feature evolves):

| # | Topic | Decision |
|---|-------|----------|
| 1 | **Latency SLO** | P95 ≤ **1000 ms** from API accept to **first spoken word** on the paging DAC (alert class: ≤ ~80 chars, `includeChime: false`, warm worker). Long-form uses streaming start SLO (see Latency SLO). |
| 2 | **Callers** | **iPhone Shortcuts** and other HTTP automations via authenticated REST API. |
| 3 | **API response timing** | Return immediately by default (`waitUntil: "accepted"`). Optional `waitUntil: "playback_started"` holds the response until speech (or chime, for chime-only) hits the DAC. |
| 4 | **Paging DAC** | **Primary Paging** assignment from existing **Audio → USB / Audio Configuration** (`usb_assignments.paging`). No duplicate DAC field in paging config. |
| 5 | **Playback gate** | **Zones must fully switch before playback.** Do not send audio to the DAC until `#PAGE1` is confirmed. |
| 6 | **Default voice** | **One bundled Piper voice** ships on install, preloaded and warm. UI supports catalog browse + remote sample preview without downloading all voices. |
| 7 | **Chimes** | Bundled default chime; user can upload custom WAV chimes; preview/set in UI. **`includeChime` defaults to `false`** on speak. Separate **chime-only API** (no TTS). |
| 8 | **Max text length** | **4000 characters.** Streaming PCM pipe for text above `streamThresholdChars` (~200). |
| 9 | **Busy behavior** | **`reject` only** for MVP (global default and per-request). Return `409 paging_busy`. Queue/replace/ignore are future work. |
| 10 | **Speech playback** | **Pipe raw PCM to DAC** (ring buffer + `aplay`), not tmpfs WAV files. Chimes remain small WAV files. |
| 11 | **API authentication** | **API key** stored hashed in DB; user creates/regenerates in **Audio → Settings**. Required for `/api/audio/paging/*` automation endpoints. |
| 12 | **Service ownership** | Native **`homepi-audio-paging`** service owns orchestration, TTS worker, and playback. **`apps/backend`** owns HTTP validation and broker publish only. **Not** owned by `homepi-audio-orchestrator` (AirPlay/PCM lifecycle). |
| 13 | **Lazy DAC** | **Always lazy:** open Primary Paging ALSA device only during an active job; close after playback + grace period. Never keep `aplay` or an ALSA handle open while idle. |
| 14 | **TTS idle policy** | **Default `always_warm`** while paging is enabled (meets alert latency SLO). Optional **`warm_with_timeout`** unloads Piper after `idleWarmTimeoutMs`. When disabled, no Piper process. |

---

## Key Decisions

### 1. Paging Uses the Primary Paging DAC (USB Assignment)

Paging audio must use the **Primary Paging** USB DAC configured in **Audio Configuration**. This DAC is independent from:

- Primary audio DAC.
- AirPlay / Shairport output.
- Spotify / PCM router output.
- Any future zone-specific audio routing.

**Source of truth:** `usb_assignments.paging` via `homepi-usb-devices` / `core/usb`.

Do **not** duplicate paging DAC selection in the Paging configuration card. The Paging UI shows the current Primary Paging device read-only and links to Audio/USB settings to change it.

Do **not** persist raw ALSA device names like `hw:2,0` as the source of truth because ALSA card numbers can change across reboots or USB re-enumeration.

Runtime flow:

```txt
usb_assignments.paging (stable USB device id)
  -> core/usb resolves current device
  -> core/usb returns current ALSA playback target
  -> paging audio player uses resolved ALSA device
```

---

### 2. Page Volume Is Managed Per Zone

Page volume does **not** belong in the Paging configuration screen.

HiFi2 has native per-zone page volume support using:

```txt
*ZzPGVOLx
```

Where:

- `z` is the zone number from 1-16, or `0` for all zones.
- `x` is the page volume level from 1-100.

The Zone section of the Home Audio module must expose page volume per zone.

Example:

```txt
Kitchen Zone
  Initial Volume: 30
  Current Volume: 42
  Page Volume: 65
```

When HiFi2 paging is active, all zones switch to source 8 and use their configured page volume.

---

### 3. Use Explicit Page On / Off Commands

Do **not** use `*PAGE2` for normal automated paging.

`*PAGE2` is toggle mode. Toggle is unsafe for automation because if state is already out of sync, HomePi could turn paging off when it intended to turn it on, or turn it on when it intended to turn it off.

Use explicit commands:

```txt
*PAGE1   // Start page
*PAGE0   // End page
```

HiFi2 page state values:

```txt
0 = off
1 = on
2 = toggle command
3 = externally/hardware initiated page
```

If HomePi sees page state `3`, it must not attempt to terminate the page because the protocol states external hardware paging cannot be terminated by software.

---

### 4. Voice Preview Must Not Require Storing Every Voice

Do not download and store all voices on the Pi.

Voice handling must be split into:

```txt
Catalog Voice   = available voice metadata; not installed locally
Installed Voice = downloaded voice files available on Pi
Active Voice    = installed voice loaded by the local TTS worker
```

The UI can display many catalog voices, but only installed voices are stored locally.

**One default voice is bundled on install** (e.g. `en_US-lessac-medium`) so paging can be ready without a first-run download.

Recommended limits:

```json
{
  "maxInstalledVoices": 2,
  "maxTextLength": 4000,
  "maxPreviewTextLength": 120,
  "streamThresholdChars": 200
}
```

The default voice must always be installed and preloaded before paging is considered ready.

---

### 5. Paging Must Be Fast — Pipe PCM, Do Not Write WAV

The live paging path must avoid cold-starting the TTS engine for every request.

Do **not** use this live path:

```txt
API call
  -> spawn Piper CLI
  -> load model
  -> generate WAV to disk
  -> spawn aplay on file
  -> speak
```

This is too slow because model loading and file I/O add latency on every request.

Preferred production path:

```txt
homepi-audio-paging service startup
  -> load paging config
  -> resolve Primary Paging DAC from core/usb
  -> start/warm TTS worker with bundled default voice
  -> mark paging ready

API call
  -> validate text + auth
  -> create job
  -> in parallel:
       A) send *PAGE1, wait for #PAGE1
       B) Piper synthesizes into in-memory ring buffer (no DAC yet)
  -> gate: #PAGE1 confirmed AND ring buffer >= minPrerollMs
  -> optional chime WAV (if includeChime)
  -> pipe PCM from ring buffer -> aplay -> paging DAC
  -> send *PAGE0
```

Key optimizations:

```txt
TTS ring-buffer fill + HiFi2 PAGE ON   (parallel)
Playback starts only after #PAGE1       (required)
PCM pipe to DAC                       (no tmpfs WAV for speech)
```

Do not wait for full utterance synthesis before starting HiFi2 page activation. Start synthesis in parallel with `*PAGE1`, but **do not open the DAC until `#PAGE1` is confirmed**.

For text longer than `streamThresholdChars`, stream Piper stdout to `aplay` after the gate so speech begins while synthesis continues.

---

### 6. Chimes Are First-Class, Optional on Speak

Chimes support attention without speech:

- A **default chime WAV** is bundled on install.
- Users may **upload custom chime WAVs** (validated format, duration, size).
- UI supports **preview**, **set active chime**, and **upload/remove**.
- **`POST /api/audio/paging/chime`** plays chime only (full page sequence, no TTS).
- On speak, **`includeChime` defaults to `false`**. Set `true` to play the active chime before speech.

Chime playback uses WAV files (small, persistent). Speech uses PCM pipe.

---

### 7. API Key Authentication for Automations

Paging automation endpoints require an API key:

- User creates/regenerates key in **Audio → Settings**.
- Key stored **hashed** in DB; raw key shown **once** on regenerate.
- Requests use `Authorization: Bearer <key>` or `X-HomePi-Paging-Key: <key>`.
- UI paging config and voice management remain session-authenticated (existing HomePi UI auth).

---

### 8. Resource Lifecycle — Efficient When Idle and Active

Paging must minimize CPU, RAM, and ALSA resource use when not actively announcing.

**Three resource tiers:**

```txt
DISABLED  — paging off; no Piper; DAC closed; broker subscription only
COLD      — paging on but Piper unloaded (warm_with_timeout after idle)
WARM      — Piper model loaded; DAC still closed until job starts
ACTIVE    — job in progress; DAC open; playback running
```

**Lazy DAC (always):**

- Do **not** keep the Primary Paging ALSA device open between jobs.
- Open ALSA (or spawn per-job `aplay`) only after `#PAGE1` is confirmed.
- Close ALSA and terminate `aplay` within `dacIdleCloseDelayMs` after playback drains.
- Follow the same open-on-demand / close-on-idle pattern used by `homepi-pcm-router` (`ensure_dac_open` / `drop_and_close_dac`).

**`aplay` (always per-job):**

- Spawn `aplay` for each playback segment (chime and/or speech).
- Never run a long-lived `aplay` waiting on stdin while idle.

**Piper warm policy (configurable):**

| Policy | Idle footprint | Alert latency |
|--------|----------------|---------------|
| `always_warm` (default) | Piper process + model in RAM | Best; meets ≤ 1 s SLO |
| `warm_with_timeout` | Unloads after `idleWarmTimeoutMs` | First page after cold idle is slower (+1–3 s model load) |

MVP default: **`always_warm`** while paging is enabled, to protect Shortcuts doorbell latency. Offer `warm_with_timeout` in UI as a power-saving option.

**Readiness vs resource state:**

- `ready: true` means **WARM** (or warming toward WARM), DAC **assigned and connected** (USB), and HiFi2 available — not that the DAC ALSA handle is open.
- `paging_not_ready` when COLD and a speak request arrives under `always_warm` should trigger background warm; under `warm_with_timeout`, document expected cold-start delay.

---

## Latency SLO

| Request class | Text length | Target (P95) | Notes |
|---------------|-------------|--------------|-------|
| **alert** | ≤ 80 chars | ≤ 1000 ms to **first speech sample** | Shortcuts doorbell-style; `includeChime: false` |
| **standard** | ≤ 500 chars | ≤ 1500 ms to first speech sample | |
| **long-form** | ≤ 4000 chars | Streaming start after `#PAGE1` | Schedule, scores; synthesis continues during playback |

Measurement point: first PCM sample of **spoken words** reaching the paging DAC (not chime, not `#PAGE1`).

Structured timing fields on every job: `requested_at`, `page_on_at`, `playback_started_at`, `playback_completed_at`, `completed_at`.

Recommended `minPrerollMs`: 100–150 ms of PCM in the ring buffer before opening the DAC (after `#PAGE1`).

---

## Protocol Requirements

The HiFi2 serial protocol has these relevant requirements:

- Commands start with `*`.
- Commands are terminated by carriage return `<CR>`.
- Responses start with `#`.
- Responses are terminated by `<CR><LF>`.
- Error responses start with `#?`.
- Multiple commands must be spaced by at least 50 ms to prevent buffer overruns.
- HiFi2 sends unsolicited status messages when state changes.
- HomePi must handle unsolicited page state responses.

Relevant paging commands:

```txt
*PAGE1   // page on
*PAGE0   // page off
*PAGE?   // query page state
```

Relevant responses:

```txt
#PAGE0   // page off
#PAGE1   // page on
#PAGE3   // external hardware page active
#?PAGE... // error
```

Relevant zone page volume commands:

```txt
*ZzPGVOLx
*ZzPGVOL?
```

Implement `*PAGE1` and `*PAGE0` in `homepi-hifi-serial` (today only `PAGE?` exists). Paging service uses typed broker commands to `modules.hifi.command`, not raw serial from the API layer.

---

## High-Level Architecture

```txt
/apps/frontend
  Home Audio module
    Paging configuration UI
    Voice browser / preview UI
    Chime management UI
    Paging status UI
  Audio → Settings
    Paging API key management

/apps/backend
  /api/audio/paging/*     HTTP validation, API key auth, broker publish
  /api/audio/settings     paging API key regenerate

/services/homepi-audio-paging
  PagingOrchestrator
  PiperWorkerManager
  PagingPcmPlayer          ring buffer + per-job aplay; lazy DAC open/close
  PagingChimePlayer        WAV chime playback; lazy DAC open/close
  PagingResourceManager    WARM/COLD/ACTIVE lifecycle, idle timers
  HifiPagingController
  PagingVoiceCatalog
  PagingJobStore
  PagingConfigStore

/core/broker
  Routes API commands to homepi-audio-paging

/core/events
  Emits paging state, lifecycle, and health events

/core/database
  Persists config, voices, chimes, job metadata, API key hash

/core/logging
  Structured logs with correlation IDs

/core/usb  (homepi-usb-devices)
  Resolves Primary Paging DAC to current ALSA playback device

/homepi-hifi-serial
  Sends *PAGE1 / *PAGE0 through existing HiFi2 command queue
```

Paging is **not** owned by `homepi-audio-orchestrator` (AirPlay / PCM / zone routing). It is a separate short-lived job service.

---

## Module Boundaries

### API Layer (`apps/backend`)

The API layer must:

- Validate request bodies and API key (for automation endpoints).
- Create or attach a `correlationId`.
- Publish commands to `core/broker`.
- Return job status per `waitUntil` policy.
- Never directly send HiFi2 serial commands.
- Never directly spawn Piper.
- Never directly call ALSA playback.

### Paging Orchestrator (`homepi-audio-paging`)

The orchestrator owns the paging state machine.

Responsibilities:

- Enforce enabled/disabled state.
- Enforce busy lock (`reject` when busy).
- Validate active voice readiness.
- Validate Primary Paging DAC readiness via `core/usb`.
- Start TTS synthesis into ring buffer.
- Start HiFi2 page activation; **wait for `#PAGE1` before DAC output**.
- Play optional chime, then pipe speech PCM.
- End paging.
- Emit events.
- Write structured job metadata.
- Guarantee page-off cleanup when HomePi started the page.
- Transition resource state WARM → ACTIVE → WARM around each job.
- Optional debug tee of PCM stream when `keepLastAudioForDebug` is enabled.

### TTS Worker Manager

The TTS worker manager owns Piper process/server lifecycle.

Responsibilities:

- Start TTS worker when paging is enabled and policy requires WARM (see Resource Lifecycle).
- Load configured default (bundled) voice.
- Keep active voice warm per `idlePolicy` (`always_warm` or `warm_with_timeout`).
- Unload Piper and transition to COLD after `idleWarmTimeoutMs` when policy is `warm_with_timeout`.
- Reload Piper on next job or preview if COLD (emit `audio.paging.warming` / `audio.paging.warm`).
- Restart worker when voice changes.
- Expose readiness state (WARM vs COLD vs warming).
- Stream raw PCM into ring buffer (no speech WAV files).
- Enforce max text length (4000) and preview text length.
- Use full-buffer mode for short text; streaming mode above `streamThresholdChars`.

When paging is **disabled**, do not load or keep a Piper process.

### PCM Audio Player

The PCM player owns speech playback through the Primary Paging DAC.

Responsibilities:

- Resolve paging DAC through `core/usb` (`usb_assignments.paging`).
- Convert stable USB ID to current ALSA device.
- **Lazy-open** ALSA only when a job reaches the playback gate (after `#PAGE1`).
- Pipe raw PCM to a **per-job** `aplay` (or ALSA API) on the resolved device.
- **Lazy-close** ALSA and terminate `aplay` within `dacIdleCloseDelayMs` after playback drains.
- Maintain ring buffer fed by Piper during `#PAGE1` wait (no DAC open during this phase).
- Report playback start/completion/failure.
- Emit `audio.paging.dac.opened` / `audio.paging.dac.closed`.
- Avoid using default system audio device.

Production path (per job, after `#PAGE1`):

```bash
# Warm Piper writes to ring buffer during PAGE ON wait; then:
aplay -D <resolved_alsa> -f S16_LE -r 22050 -c 1 -t raw < <(pipe from ring buffer + Piper stdout)
```

- Long-lived **Piper** worker is preferred over per-request CLI spawn.
- Long-lived **`aplay`** is **not** used — spawn per playback segment only.

Expected lazy-open overhead: ~20–80 ms per job (acceptable vs TTS and `#PAGE1`).

### Chime Player

The chime player owns short WAV playback through the same paging DAC.

Responsibilities:

- Play bundled or user-uploaded chime WAV before speech (when `includeChime: true`).
- Play chime-only jobs (`POST /api/audio/paging/chime`).
- Chime preview through paging DAC only (no HiFi PAGE in preview mode).
- Use the same lazy DAC open/close as PCM player (per-job `aplay`, close after grace).

### HiFi Paging Controller

The HiFi paging controller owns interaction with the existing HiFi2 controller.

Responsibilities:

- Send `*PAGE1`.
- Send `*PAGE0`.
- Query `*PAGE?` when needed.
- Wait for `#PAGE1` confirmation or timeout.
- Detect `#PAGE3` external page state.
- Never use `*PAGE2` for automated paging.
- Respect the existing HiFi2 command queue and 50 ms spacing rules.

### Paging Resource Manager

The resource manager owns idle efficiency and DAC lifecycle.

Responsibilities:

- Track resource state: `DISABLED`, `COLD`, `WARM`, `ACTIVE`.
- Enforce `idlePolicy` (`always_warm` vs `warm_with_timeout`).
- Start `idleWarmTimeoutMs` timer after job completion; unload Piper and emit `audio.paging.cold` on expiry.
- Cancel idle timer when a new job arrives or policy changes to `always_warm`.
- Coordinate lazy DAC open (delegated to PCM/Chime players) and verify DAC is closed when not ACTIVE.
- Expose `resourceState` and `dacOpen` in status snapshots.
- Align with `homepi-pcm-router` patterns (`ensure_dac_open` / `drop_and_close_dac` on demand).

**Idle resource footprint targets:**

| State | Piper | ALSA / aplay | Expected CPU |
|-------|-------|--------------|--------------|
| DISABLED | off | closed | ~0% |
| COLD | off | closed | ~0% |
| WARM | loaded | closed | low (process idle) |
| ACTIVE | inferencing/playing | open | burst during job |

---

## Runtime State Machine

### Job states

Paging job states:

```txt
DISABLED
NOT_READY
READY
BUSY
TTS_GENERATING
PAGE_STARTING
PLAYING
PAGE_ENDING
FAILED
```

### Resource lifecycle states

Separate from job states; tracks idle efficiency:

```txt
DISABLED  — paging feature disabled
COLD      — enabled; Piper unloaded (warm_with_timeout expired)
WARM      — enabled; Piper loaded; DAC closed
ACTIVE    — DAC open; playback in progress (subset of BUSY job states)
```

Transitions:

```txt
DISABLED -> WARM          (paging enabled, always_warm policy)
DISABLED -> COLD          (paging enabled, warm_with_timeout; load on first job)
WARM -> COLD              (idleWarmTimeoutMs elapsed, warm_with_timeout policy)
COLD -> WARM              (job or preview requested; Piper loading)
WARM -> ACTIVE            (job passes #PAGE1 gate; DAC opened)
ACTIVE -> WARM            (playback drained + dacIdleCloseDelayMs; Piper stays warm)
WARM -> DISABLED          (paging disabled)
```

Readiness dependencies:

```txt
Paging enabled
Primary Paging DAC assigned (usb_assignments.paging)
Primary Paging DAC currently connected (USB; ALSA handle may be closed)
Default (bundled) voice installed
TTS worker in WARM state (or warming toward WARM)
HiFi2 controller connected
No active external page (#PAGE3)
No current HomePi page job
```

`ready: true` does **not** require the DAC ALSA device to be open.

Example state transitions:

```txt
DISABLED -> READY
READY -> TTS_GENERATING
TTS_GENERATING -> PAGE_STARTING
PAGE_STARTING -> PLAYING
PLAYING -> PAGE_ENDING
PAGE_ENDING -> READY
```

Failure transition example:

```txt
PLAYING -> FAILED -> PAGE_ENDING -> READY
```

If page-off fails, emit a critical event and keep state as `FAILED` until the page state is confirmed off or manually cleared.

---

## Low-Latency Speak Sequence

Production sequence:

```txt
POST /api/audio/paging/speak
Authorization: Bearer <paging-api-key>

1. Validate API key, text, and config.
2. Create job and correlationId.
3. Acquire paging busy lock (reject if busy).
4. Emit audio.paging.requested.
5. In parallel:
     A) Send HiFi2 *PAGE1; wait for #PAGE1 confirmation
     B) Start Piper synthesis into in-memory ring buffer (no DAC output yet)
6. Gate: #PAGE1 confirmed AND ring buffer >= minPrerollMs
7. Open paging DAC (lazy-open ALSA / spawn per-job aplay).
8. If includeChime (default false): play chime WAV, wait for completion
9. Pipe PCM from ring buffer to aplay on Primary Paging DAC; stream until Piper EOF
10. Wait for playback drain.
11. Close paging DAC (lazy-close within dacIdleCloseDelayMs).
12. Send HiFi2 *PAGE0.
13. Confirm page off if possible.
14. Release busy lock.
15. Emit audio.paging.completed.
16. Return HTTP response per waitUntil policy.
```

Pseudo-code:

```ts
async function speak(request: PagingSpeakRequest): Promise<PagingJobResult> {
  const correlationId = createCorrelationId('audio.paging');
  const job = await jobStore.createJob({ request, correlationId });

  await busyLock.acquireOrThrow('paging_busy');

  let homePiStartedPage = false;

  try {
    assertPagingEnabled();
    assertTtsReady();
    assertPagingDacReady();
    assertHifiReady();

    events.emit('audio.paging.requested', { correlationId, jobId: job.id });

    const ringBuffer = ttsWorker.startSynthesis({
      text: request.text,
      voiceId: request.voiceId ?? config.defaultVoiceId,
      correlationId,
      stream: request.text.length > config.streamThresholdChars,
    });

    const pageOnPromise = hifiPaging.startPage({ correlationId }).then(() => {
      homePiStartedPage = true;
    });

    await pageOnPromise;
    await ringBuffer.waitForPreroll(config.minPrerollMs);

    if (request.includeChime ?? false) {
      await chimePlayer.playWav({
        chimeId: request.chimeId ?? config.activeChimeId,
        correlationId,
      });
    }

    await pcmPlayer.pipeFromRingBuffer({
      ringBuffer,
      dacDeviceId: pagingDacFromUsbAssignments(),
      correlationId,
      onDacOpened: () => events.emit('audio.paging.dac.opened', { correlationId }),
      onPlaybackStarted: () => maybeResolveWaitUntil(request),
      onDacClosed: () => events.emit('audio.paging.dac.closed', { correlationId }),
    });

    await hifiPaging.endPage({ correlationId });

    events.emit('audio.paging.completed', { correlationId, jobId: job.id });

    return { ok: true, jobId: job.id, correlationId, status: 'completed' };
  } catch (error) {
    events.emit('audio.paging.failed', {
      correlationId,
      jobId: job.id,
      error: normalizeError(error),
    });

    if (homePiStartedPage) {
      try {
        await hifiPaging.endPage({ correlationId });
      } catch (pageOffError) {
        logger.error('Failed to end HiFi2 page after paging failure', {
          correlationId,
          pageOffError,
        });
      }
    }

    throw error;
  } finally {
    busyLock.release();
  }
}
```

### Chime-Only Sequence

```txt
POST /api/audio/paging/chime

1. Validate API key and config.
2. Acquire busy lock (reject if busy).
3. Send *PAGE1; wait for #PAGE1.
4. Play chime WAV through Primary Paging DAC.
5. Send *PAGE0.
6. Release busy lock.
```

No Piper involvement.

---

## iPhone Shortcuts Integration

Example doorbell automation:

```http
POST https://<homepi-host>/api/audio/paging/speak
Authorization: Bearer <paging-api-key>
Content-Type: application/json
```

```json
{
  "text": "There is someone at the front door.",
  "source": "shortcuts",
  "includeChime": false,
  "onBusy": "reject",
  "waitUntil": "playback_started"
}
```

Success (after speech hits DAC when `waitUntil: "playback_started"`):

```json
{
  "ok": true,
  "jobId": "page_01JXYZ",
  "correlationId": "homepi:paging:01JXYZ",
  "status": "playing"
}
```

Double doorbell while busy:

```json
{
  "ok": false,
  "error": "paging_busy"
}
```

---

## Voice Preview Design

### Voice Catalog Preview

The config UI should allow browsing catalog voices without downloading them.

For uninstalled voices, preview should use remote/static sample audio in the browser, not local TTS generation on the Pi.

Flow:

```txt
User opens voice browser
  -> HomePi loads voice catalog metadata
  -> UI shows voice list
  -> User clicks preview
  -> Browser plays remote/static sample audio
  -> No model download happens
```

This avoids filling the Pi with unused voices.

### Installed Voice Preview

Installed voice preview uses the local TTS worker.

Flow:

```txt
User clicks local preview for installed voice
  -> API validates short preview text
  -> TTS worker synthesizes into PCM pipe
  -> PCM player outputs through Primary Paging DAC only
  -> HiFi2 PAGE mode is not activated
```

Endpoint:

```txt
POST /api/audio/paging/voices/preview
```

Request:

```json
{
  "voiceId": "en_US-lessac-medium",
  "text": "This is a HomePi paging preview.",
  "output": "paging_dac_only"
}
```

### Whole-House Preview

Whole-house preview must be explicit because it interrupts the house.

Endpoint:

```txt
POST /api/audio/paging/preview-page
```

This endpoint runs the full page sequence:

```txt
PCM pipe (TTS) -> *PAGE1 -> play through paging DAC -> *PAGE0
```

The UI should show a confirmation before calling this endpoint.

---

## Chime Management

### Storage

```txt
/var/lib/homepi/paging/chimes/
  default.wav              # bundled, required
  <user-chime-id>.wav      # user uploads
```

Validation on upload:

- WAV format only.
- Max duration: 3 seconds (configurable).
- Max file size: 500 KB (configurable).

### UI

Chime section in Paging configuration:

```txt
Active Chime
Upload Chime
Preview Chime (paging DAC only, no HiFi PAGE)
Remove custom chime
```

### Preview

```txt
POST /api/audio/paging/chimes/preview
```

Plays selected chime through Primary Paging DAC only. Does not activate HiFi PAGE mode.

---

## Voice Installation Strategy

Installed voices are stored in:

```txt
/var/lib/homepi/paging/voices/
```

Example:

```txt
/var/lib/homepi/paging/voices/en_US-lessac-medium.onnx
/var/lib/homepi/paging/voices/en_US-lessac-medium.onnx.json
```

**Bundled on install:** default voice files are shipped in the HomePi image under `/var/lib/homepi/paging/voices/`.

Generated speech is **not** written to tmpfs or SD card in normal operation. PCM streams directly to the DAC. Optional debug tee when `keepLastAudioForDebug` is enabled:

```txt
/var/lib/homepi/paging/debug/last-page.raw
```

When installing an additional voice:

```txt
1. Check max installed voice limit.
2. Download model and config to temp path.
3. Verify files exist and are readable.
4. Move files into /var/lib/homepi/paging/voices.
5. Insert/update AUDIO_PAGING_VOICES.
6. If set as default, reload TTS worker.
7. Emit audio.paging.voice.installed.
```

When changing the default voice:

```txt
1. Confirm voice is installed.
2. Update AUDIO_PAGING_CONFIG.DEFAULT_VOICE_ID.
3. Set previous voice IS_DEFAULT = 0.
4. Set new voice IS_DEFAULT = 1.
5. Restart/reload TTS worker with new voice.
6. Emit audio.paging.voice.changed.
7. Emit audio.paging.ready when worker is warm.
```

Paging API must reject live requests while the voice is warming:

```json
{
  "ok": false,
  "error": "paging_voice_not_ready"
}
```

---

## Database Schema

All schema changes must go through `core/database` migrations.

### AUDIO_PAGING_CONFIG

```sql
CREATE TABLE IF NOT EXISTS AUDIO_PAGING_CONFIG (
  ID INTEGER PRIMARY KEY CHECK (ID = 1),

  ENABLED INTEGER NOT NULL DEFAULT 0,

  DEFAULT_VOICE_ID TEXT,
  ACTIVE_VOICE_ID TEXT,

  ACTIVE_CHIME_ID TEXT NOT NULL DEFAULT 'default',

  MAX_INSTALLED_VOICES INTEGER NOT NULL DEFAULT 2,
  MAX_TEXT_LENGTH INTEGER NOT NULL DEFAULT 4000,
  MAX_PREVIEW_TEXT_LENGTH INTEGER NOT NULL DEFAULT 120,
  STREAM_THRESHOLD_CHARS INTEGER NOT NULL DEFAULT 200,
  MIN_PREROLL_MS INTEGER NOT NULL DEFAULT 100,

  DEFAULT_ON_BUSY TEXT NOT NULL DEFAULT 'reject',

  IDLE_POLICY TEXT NOT NULL DEFAULT 'always_warm',
  IDLE_WARM_TIMEOUT_MS INTEGER NOT NULL DEFAULT 1800000,
  DAC_IDLE_CLOSE_DELAY_MS INTEGER NOT NULL DEFAULT 3000,

  KEEP_LAST_AUDIO_FOR_DEBUG INTEGER NOT NULL DEFAULT 0,

  CREATED_AT TEXT NOT NULL,
  UPDATED_AT TEXT NOT NULL
);
```

Rules:

- Only one row exists: `ID = 1`.
- **No `PAGING_DAC_DEVICE_ID`** — DAC comes from `usb_assignments.paging`.
- `DEFAULT_VOICE_ID` references bundled or installed voice.
- `DEFAULT_ON_BUSY` supports only `reject` in MVP.
- `IDLE_POLICY`: `always_warm` (default) or `warm_with_timeout`.
- `IDLE_WARM_TIMEOUT_MS`: time after last job before unloading Piper when `warm_with_timeout` (default 30 minutes).
- `DAC_IDLE_CLOSE_DELAY_MS`: time after playback drain before closing ALSA handle (default 3 seconds).

---

### AUDIO_PAGING_VOICES

```sql
CREATE TABLE IF NOT EXISTS AUDIO_PAGING_VOICES (
  VOICE_ID TEXT PRIMARY KEY,

  DISPLAY_NAME TEXT NOT NULL,
  LANGUAGE_CODE TEXT NOT NULL,
  QUALITY TEXT,

  MODEL_PATH TEXT NOT NULL,
  CONFIG_PATH TEXT NOT NULL,

  INSTALLED INTEGER NOT NULL DEFAULT 1,
  IS_DEFAULT INTEGER NOT NULL DEFAULT 0,
  IS_BUNDLED INTEGER NOT NULL DEFAULT 0,

  LAST_USED_AT TEXT,

  CREATED_AT TEXT NOT NULL,
  UPDATED_AT TEXT NOT NULL
);
```

Rules:

- Only installed voices are stored here.
- Catalog-only voices should not be inserted.
- Only one voice should have `IS_DEFAULT = 1`.
- Bundled voice has `IS_BUNDLED = 1`.

---

### AUDIO_PAGING_CHIMES

```sql
CREATE TABLE IF NOT EXISTS AUDIO_PAGING_CHIMES (
  CHIME_ID TEXT PRIMARY KEY,

  DISPLAY_NAME TEXT NOT NULL,
  FILE_PATH TEXT NOT NULL,
  DURATION_MS INTEGER,

  IS_DEFAULT INTEGER NOT NULL DEFAULT 0,
  IS_BUNDLED INTEGER NOT NULL DEFAULT 0,

  CREATED_AT TEXT NOT NULL,
  UPDATED_AT TEXT NOT NULL
);
```

Rules:

- Bundled `default` chime row exists on first migration.
- Only one chime should have `IS_DEFAULT = 1`.
- User uploads get generated `chime_id` values.

---

### AUDIO_PAGING_API_KEY

```sql
CREATE TABLE IF NOT EXISTS AUDIO_PAGING_API_KEY (
  ID INTEGER PRIMARY KEY CHECK (ID = 1),

  KEY_HASH TEXT NOT NULL,
  KEY_PREFIX TEXT NOT NULL,

  CREATED_AT TEXT NOT NULL,
  UPDATED_AT TEXT NOT NULL
);
```

Rules:

- Only one row: `ID = 1`.
- Store bcrypt/argon2 hash only; never store raw key after initial display.
- `KEY_PREFIX` (first 8 chars) shown in UI for identification.

---

### AUDIO_PAGING_JOBS

```sql
CREATE TABLE IF NOT EXISTS AUDIO_PAGING_JOBS (
  JOB_ID TEXT PRIMARY KEY,
  CORRELATION_ID TEXT NOT NULL,

  STATUS TEXT NOT NULL,
  SOURCE TEXT NOT NULL,
  JOB_TYPE TEXT NOT NULL DEFAULT 'speak',

  TEXT_HASH TEXT,
  TEXT_LENGTH INTEGER,

  VOICE_ID TEXT,
  CHIME_ID TEXT,
  INCLUDE_CHIME INTEGER NOT NULL DEFAULT 0,

  REQUESTED_AT TEXT NOT NULL,
  STARTED_AT TEXT,
  PAGE_ON_AT TEXT,
  PLAYBACK_STARTED_AT TEXT,
  PLAYBACK_COMPLETED_AT TEXT,
  COMPLETED_AT TEXT,

  ERROR_STAGE TEXT,
  ERROR_MESSAGE TEXT,

  CREATED_AT TEXT NOT NULL,
  UPDATED_AT TEXT NOT NULL
);
```

Rules:

- Do not store raw page text by default.
- Store `TEXT_HASH` and `TEXT_LENGTH` for speak jobs.
- `JOB_TYPE` is `speak` or `chime`.
- No `AUDIO_FILE` column — speech is piped, not file-based.
- Prune old job rows after retention period (e.g. 30 days).

---

## API Endpoints

All API routes must validate schema before publishing commands or writing config.

Automation endpoints (`/speak`, `/chime`) require paging API key.

### Paging API Key (Audio Settings)

```txt
GET  /api/audio/settings
```

Response includes:

```json
{
  "pagingApiKeyConfigured": true,
  "pagingApiKeyPrefix": "hpi_a1b2"
}
```

Never return the raw key on GET.

```txt
POST /api/audio/settings/paging-api-key/regenerate
```

Response (raw key shown once):

```json
{
  "ok": true,
  "apiKey": "hpi_a1b2c3d4...",
  "prefix": "hpi_a1b2"
}
```

```txt
DELETE /api/audio/settings/paging-api-key
```

Disables automation until a new key is generated.

---

### Get Paging Config

```txt
GET /api/audio/paging/config
```

Response:

```json
{
  "enabled": true,
  "pagingDacDeviceId": "usb-stable-id",
  "pagingDacSource": "usb_assignments.paging",
  "defaultVoiceId": "en_US-lessac-medium",
  "activeVoiceId": "en_US-lessac-medium",
  "activeChimeId": "default",
  "maxInstalledVoices": 2,
  "maxTextLength": 4000,
  "maxPreviewTextLength": 120,
  "streamThresholdChars": 200,
  "defaultOnBusy": "reject",
  "idlePolicy": "always_warm",
  "idleWarmTimeoutMs": 1800000,
  "dacIdleCloseDelayMs": 3000,
  "keepLastAudioForDebug": false,
  "status": {
    "ready": true,
    "resourceState": "WARM",
    "dacConnected": true,
    "dacOpen": false,
    "voiceLoaded": true,
    "hifiConnected": true,
    "busy": false
  }
}
```

Note: `pagingDacDeviceId` is read from USB assignments (read-only in paging UI).

---

### Update Paging Config

```txt
PUT /api/audio/paging/config
```

Request:

```json
{
  "enabled": true,
  "defaultVoiceId": "en_US-lessac-medium",
  "activeChimeId": "default",
  "idlePolicy": "always_warm",
  "idleWarmTimeoutMs": 1800000,
  "dacIdleCloseDelayMs": 3000
}
```

Behavior:

- Persist settings through `core/database`.
- Paging DAC is **not** set here — use Audio/USB configuration.
- If `idlePolicy` changes to `warm_with_timeout`, start idle timer after next job completes.
- If `idlePolicy` is `always_warm` and currently COLD, reload Piper immediately.
- If default voice changes, reload/warm TTS worker.
- Emit updated readiness state.

---

### Get Voice Catalog

```txt
GET /api/audio/paging/voices/catalog
```

Response:

```json
{
  "voices": [
    {
      "voiceId": "en_US-lessac-medium",
      "displayName": "Lessac",
      "languageCode": "en_US",
      "quality": "medium",
      "installed": true,
      "isDefault": true,
      "isBundled": true,
      "sampleAvailable": true,
      "sampleUrl": "optional-browser-sample-url"
    }
  ]
}
```

Rules:

- Catalog may include voices not installed locally.
- Do not insert catalog-only voices into `AUDIO_PAGING_VOICES`.
- Include `installed` by joining catalog metadata with local DB state.

---

### Install Voice

```txt
POST /api/audio/paging/voices/install
```

Request:

```json
{
  "voiceId": "en_US-lessac-medium",
  "setDefault": true
}
```

Response:

```json
{
  "ok": true,
  "voiceId": "en_US-lessac-medium",
  "installed": true,
  "isDefault": true,
  "workerReloading": true
}
```

Rules:

- Enforce max installed voice count.
- If limit reached, return a structured error requiring replacement.

Example error:

```json
{
  "ok": false,
  "error": "max_installed_voices_reached",
  "maxInstalledVoices": 2,
  "installedVoices": [
    "en_US-lessac-medium",
    "en_US-amy-low"
  ]
}
```

---

### Remove Voice

```txt
DELETE /api/audio/paging/voices/:voiceId
```

Rules:

- Cannot remove bundled default voice.
- Cannot remove active/default voice unless another installed voice is supplied as replacement.
- Stop/reload TTS worker if removing active voice.
- Delete model/config files from `/var/lib/homepi/paging/voices` (unless bundled).
- Update DB.

---

### Chime Endpoints

```txt
GET  /api/audio/paging/chimes
POST /api/audio/paging/chimes/upload
POST /api/audio/paging/chimes/preview
PUT  /api/audio/paging/chimes/:chimeId/active
DELETE /api/audio/paging/chimes/:chimeId
```

---

### Preview Installed Voice Through Paging DAC Only

```txt
POST /api/audio/paging/voices/preview
```

Request:

```json
{
  "voiceId": "en_US-lessac-medium",
  "text": "This is a HomePi paging preview.",
  "output": "paging_dac_only"
}
```

Rules:

- Does not activate HiFi2 page mode.
- Uses Primary Paging DAC only.
- Requires installed voice.
- Enforces `MAX_PREVIEW_TEXT_LENGTH`.
- Uses PCM pipe (no tmpfs file).

---

### Whole-House Paging Preview

```txt
POST /api/audio/paging/preview-page
```

Request:

```json
{
  "voiceId": "en_US-lessac-medium",
  "text": "This is a whole house paging preview.",
  "includeChime": false
}
```

Rules:

- Runs full page sequence.
- UI must require confirmation before calling this endpoint.
- Uses same safety rules as live paging.

---

### Speak

```txt
POST /api/audio/paging/speak
Authorization: Bearer <paging-api-key>
```

Request:

```json
{
  "text": "There is someone at the front door.",
  "voiceId": null,
  "source": "shortcuts",
  "includeChime": false,
  "chimeId": null,
  "onBusy": "reject",
  "waitUntil": "accepted"
}
```

| Field | Default | Notes |
|-------|---------|-------|
| `includeChime` | **`false`** | Play active chime before speech when `true` |
| `chimeId` | active chime from config | Override when `includeChime: true` |
| `onBusy` | **`reject`** | Only supported value in MVP |
| `waitUntil` | **`accepted`** | `playback_started` waits until speech hits DAC |
| `voiceId` | configured default | Optional override |

Response (`waitUntil: "accepted"`):

```json
{
  "ok": true,
  "jobId": "page_01JXYZ",
  "correlationId": "homepi:paging:01JXYZ",
  "status": "started"
}
```

Response (`waitUntil: "playback_started"`):

```json
{
  "ok": true,
  "jobId": "page_01JXYZ",
  "correlationId": "homepi:paging:01JXYZ",
  "status": "playing"
}
```

Busy response:

```json
{
  "ok": false,
  "error": "paging_busy"
}
```

Unauthorized:

```json
{
  "ok": false,
  "error": "unauthorized"
}
```

Not ready response:

```json
{
  "ok": false,
  "error": "paging_not_ready",
  "details": {
    "dacConnected": false,
    "voiceLoaded": true,
    "hifiConnected": true
  }
}
```

---

### Chime Only

```txt
POST /api/audio/paging/chime
Authorization: Bearer <paging-api-key>
```

Request:

```json
{
  "chimeId": "default",
  "source": "shortcuts",
  "onBusy": "reject",
  "waitUntil": "accepted"
}
```

Response:

```json
{
  "ok": true,
  "jobId": "chime_01JXYZ",
  "correlationId": "homepi:paging:chime:01JXYZ",
  "status": "started"
}
```

Rules:

- No TTS. Full page sequence: `*PAGE1` → chime WAV → `*PAGE0`.
- Same auth, busy, and readiness rules as speak.

---

### Get Paging Status

```txt
GET /api/audio/paging/status
```

Response:

```json
{
  "state": "READY",
  "ready": true,
  "resourceState": "WARM",
  "busy": false,
  "currentJobId": null,
  "dependencies": {
    "enabled": true,
    "pagingDacConfigured": true,
    "pagingDacConnected": true,
    "dacOpen": false,
    "defaultVoiceInstalled": true,
    "ttsWorkerReady": true,
    "hifiConnected": true
  }
}
```

`resourceState` is one of: `DISABLED`, `COLD`, `WARM`, `ACTIVE`. `dacOpen` reflects whether the ALSA handle is currently open (true only during ACTIVE jobs).

---

## Broker Commands

### Speak Command

```json
{
  "type": "audio.paging.command.speak",
  "correlationId": "homepi:paging:01JXYZ",
  "source": "api",
  "payload": {
    "text": "There is someone at the front door.",
    "voiceId": null,
    "includeChime": false,
    "chimeId": null,
    "onBusy": "reject",
    "waitUntil": "accepted"
  }
}
```

### Chime Command

```json
{
  "type": "audio.paging.command.chime",
  "correlationId": "homepi:paging:chime:01JXYZ",
  "source": "api",
  "payload": {
    "chimeId": "default",
    "onBusy": "reject",
    "waitUntil": "accepted"
  }
}
```

### Preview Command

```json
{
  "type": "audio.paging.command.preview_voice",
  "correlationId": "homepi:paging:preview:01JXYZ",
  "source": "api",
  "payload": {
    "text": "This is a HomePi paging preview.",
    "voiceId": "en_US-lessac-medium",
    "output": "paging_dac_only"
  }
}
```

### Reload Voice Command

```json
{
  "type": "audio.paging.command.reload_voice",
  "correlationId": "homepi:paging:voice:01JXYZ",
  "source": "api",
  "payload": {
    "voiceId": "en_US-lessac-medium"
  }
}
```

---

## Events

Paging module must emit events through `core/events`.

### Readiness Events

```txt
audio.paging.ready
audio.paging.not_ready
audio.paging.disabled
audio.paging.dependency_changed
```

### Resource Lifecycle Events

```txt
audio.paging.warming
audio.paging.warm
audio.paging.cold
audio.paging.dac.opened
audio.paging.dac.closed
```

- `audio.paging.warm` — Piper loaded; resource state WARM.
- `audio.paging.cold` — Piper unloaded after idle timeout.
- `audio.paging.dac.opened` — ALSA handle opened for job playback.
- `audio.paging.dac.closed` — ALSA handle closed after idle grace.

### Voice Events

```txt
audio.paging.voice.install_started
audio.paging.voice.installed
audio.paging.voice.install_failed
audio.paging.voice.removed
audio.paging.voice.changed
audio.paging.voice.warming
audio.paging.voice.ready
```

### Chime Events

```txt
audio.paging.chime.uploaded
audio.paging.chime.removed
audio.paging.chime.changed
```

### Job Lifecycle Events

```txt
audio.paging.requested
audio.paging.busy
audio.paging.tts.started
audio.paging.tts.buffer_ready
audio.paging.hifi.page_on_requested
audio.paging.hifi.page_on_confirmed
audio.paging.chime.started
audio.paging.chime.completed
audio.paging.playback.started
audio.paging.playback.completed
audio.paging.hifi.page_off_requested
audio.paging.hifi.page_off_confirmed
audio.paging.completed
audio.paging.failed
audio.paging.cancelled
```

Example event:

```json
{
  "type": "audio.paging.playback.started",
  "correlationId": "homepi:paging:01JXYZ",
  "timestamp": "2026-06-23T20:00:00.000Z",
  "source": "homepi-audio-paging",
  "payload": {
    "jobId": "page_01JXYZ",
    "jobType": "speak",
    "voiceId": "en_US-lessac-medium",
    "dacDeviceId": "usb-stable-id",
    "alsaDevice": "hw:2,0"
  }
}
```

---

## Logging Requirements

All logs must use `core/logging`.

Every paging job must have a `correlationId`.

Log levels:

```txt
INFO  - normal lifecycle events
WARN  - recoverable issues, timeout fallback, missing optional preview sample
ERROR - failed TTS, failed playback, failed page command
FATAL - paging stuck on and cannot be cleared
```

Example logs:

```json
{
  "level": "INFO",
  "service": "homepi-audio-paging",
  "event": "audio.paging.requested",
  "correlationId": "homepi:paging:01JXYZ",
  "message": "Paging request accepted",
  "data": {
    "jobId": "page_01JXYZ",
    "textLength": 38,
    "voiceId": "en_US-lessac-medium",
    "includeChime": false,
    "source": "shortcuts"
  }
}
```

```json
{
  "level": "ERROR",
  "service": "homepi-audio-paging",
  "event": "audio.paging.failed",
  "correlationId": "homepi:paging:01JXYZ",
  "message": "Paging playback failed",
  "data": {
    "jobId": "page_01JXYZ",
    "stage": "playback",
    "exitCode": 1,
    "dacDeviceId": "usb-stable-id"
  }
}
```

---

## UI Requirements

The Paging configuration section belongs inside the Home Audio module.

### Audio → Settings

```txt
Paging API Key
  [Generate / Regenerate Key]
  Shows key prefix when configured (e.g. hpi_a1b2...)
  Raw key displayed once on regenerate — copy for Shortcuts
```

### Paging Card

Fields:

```txt
Enable Paging
Primary Paging DAC (read-only, link to Audio/USB settings)
Default Voice
Installed Voices
Active Chime
Chime Upload / Preview
Power Saving (idle policy: always warm / warm with timeout)
Debug: Keep Last Audio
```

Status indicators:

```txt
Paging Ready
Resource State (WARM / COLD / ACTIVE)
DAC Connected (USB)
DAC Open (only during active job)
Voice Loaded
HiFi Connected
Busy / Idle
```

Note: Page volume is per-zone (existing zone UI). Queue behavior is `reject` only — no UI toggle needed in MVP.

### Voice Browser

Voice rows should show:

```txt
Voice Name
Language
Quality
Installed / Not Installed / Bundled
Preview Sample
Install
Set Default
Remove
```

Preview behavior:

```txt
Uninstalled voice -> browser plays sample audio only
Installed voice -> optional local preview through paging DAC
Whole-house preview -> explicit button with confirmation
```

### Zone Page Volume

Zone cards/edit forms must include:

```txt
Page Volume
```

This controls:

```txt
*ZzPGVOLx
```

Do not duplicate page volume in the Paging config card.

### UX Notes

Follow existing HomePi UI direction:

- Clean Apple-like cards.
- Light and dark mode compatible.
- Soft status pills.
- Avoid dense technical labels unless expanded.
- Use clear warnings for whole-house preview.
- Make readiness issues obvious.

Example status messaging:

```txt
Paging is not ready because the Primary Paging DAC is disconnected.
```

```txt
Voice is warming. Paging will be available when the voice is loaded.
```

```txt
Power saving is on. The first page after idle may take a few seconds while the voice loads.
```

---

## Error Handling

### Busy

MVP supports only:

```txt
reject
```

If another page is active, reject the request.

```json
{
  "ok": false,
  "error": "paging_busy"
}
```

Future (non-goals for MVP): `ignore`, `replace`, `queue`, `dedupeKey`.

### Unauthorized

Missing or invalid API key on automation endpoints:

```json
{
  "ok": false,
  "error": "unauthorized"
}
```

### Missing DAC

If Primary Paging DAC is unassigned or disconnected:

```json
{
  "ok": false,
  "error": "paging_dac_missing"
}
```

Emit:

```txt
audio.paging.not_ready
```

### Voice Not Ready

If default voice is not installed or worker is warming:

```json
{
  "ok": false,
  "error": "paging_voice_not_ready"
}
```

### HiFi2 Not Connected

If HiFi2 controller is disconnected:

```json
{
  "ok": false,
  "error": "hifi_not_connected"
}
```

### External Hardware Page Active

If `#PAGE3` is active:

```json
{
  "ok": false,
  "error": "external_page_active"
}
```

HomePi must not send `*PAGE0` to terminate a hardware-initiated page.

### Playback Failure

If audio playback fails after HomePi already started the page:

```txt
1. Log playback failure.
2. Emit audio.paging.failed.
3. Attempt *PAGE0 if HomePi started the page.
4. Mark job failed.
5. Release busy lock.
```

---

## Timeouts

Recommended initial values:

```json
{
  "pageOnConfirmTimeoutMs": 750,
  "pageOffConfirmTimeoutMs": 1000,
  "ttsGenerationTimeoutMs": 60000,
  "playbackStartTimeoutMs": 2000,
  "playbackMaxDurationMs": 300000,
  "minPrerollMs": 100,
  "idleWarmTimeoutMs": 1800000,
  "dacIdleCloseDelayMs": 3000
}
```

- `ttsGenerationTimeoutMs` and `playbackMaxDurationMs` are elevated for long-form (up to 4000 chars).
- `idleWarmTimeoutMs` applies only when `idlePolicy` is `warm_with_timeout`.
- `dacIdleCloseDelayMs` is the grace period after playback drain before closing the ALSA handle.
- Do not block indefinitely on any subprocess or serial response.

---

## Filesystem Layout

Persistent files:

```txt
/var/lib/homepi/paging/
  voices/
    <voice-id>.onnx
    <voice-id>.onnx.json
  chimes/
    default.wav
    <user-chime-id>.wav
```

No tmpfs job audio directory for normal operation (PCM is piped).

Debug files, only if enabled:

```txt
/var/lib/homepi/paging/debug/
  last-page.raw
```

Permissions:

- HomePi service user must be able to read installed voices and chimes.
- HomePi service user must have permission to access Primary Paging ALSA device.
- HomePi service user must not require root for normal paging.

---

## Service Startup Behavior

On `homepi-audio-paging` startup:

```txt
1. Load paging config from core/database.
2. If paging disabled, state = DISABLED; do not load Piper; do not open DAC.
3. If enabled, resolve Primary Paging DAC from usb_assignments via core/usb (verify connected; do not open ALSA).
4. Validate bundled/default voice exists in AUDIO_PAGING_VOICES.
5. If idlePolicy = always_warm: start TTS worker, wait for WARM.
6. If idlePolicy = warm_with_timeout: remain COLD until first job or preview.
7. Verify HiFi2 controller connection state from core/events.
8. Emit audio.paging.ready or audio.paging.not_ready.
```

While idle (WARM):

```txt
- Piper process may be loaded (always_warm) or unloaded (COLD).
- No ALSA handle open on paging DAC.
- No aplay process running.
- Service listens on core/events broker only (minimal CPU).
```

No polling loop should be added for readiness or idle timeout.

Idle and readiness changes should be driven by:

- `core/usb` device connected/disconnected events (Primary Paging assignment).
- TTS worker lifecycle events (WARM / COLD / warming).
- Job completion → start `idleWarmTimeoutMs` timer (warm_with_timeout only).
- Idle timer expiry → unload Piper, emit `audio.paging.cold`.
- DAC close timer (`dacIdleCloseDelayMs`) after playback drain.
- HiFi2 controller connection events.
- Config update events.
- Service lifecycle events from `core/events`.

---

## Resource Budget (Raspberry Pi 5)

Planning targets for SD image sizing, RAM headroom, and CPU expectations. Use during implementation and install script design.

### SD card storage

| Item | Size (approx.) | Notes |
|------|----------------|--------|
| Bundled voice (`.onnx` + `.json`) | 60–65 MB | Default: `en_US-lessac-medium` |
| Second installed voice (max 2) | 15–65 MB each | `low` ≈ 5–15 MB; `medium` ≈ 40–65 MB |
| Piper binary (ARM64) | 5–15 MB | Install dependency |
| `homepi-audio-paging` binary | 1–5 MB | Native service |
| Chimes (default + user uploads) | &lt; 1–2 MB | Max 500 KB per upload, ≤ 3 s duration |
| Job metadata (SQLite) | &lt; 1 MB | Pruned after retention period; no raw text |
| Generated speech (normal operation) | **0** | PCM pipe — no job audio on SD |

| Configuration | Total SD impact |
|---------------|-----------------|
| **Baseline** (bundled voice only) | **~75–85 MB** |
| **Maximum** (2 medium voices + chimes) | **~130–150 MB** |
| Debug mode (`keepLastAudioForDebug`) | +few MB occasional `last-page.raw` |

Catalog voices are not stored locally unless installed. Do not download the full Piper catalog to the Pi.

### RAM

| Resource state | Extra RAM (approx.) |
|----------------|---------------------|
| **DISABLED** | ~0 (no Piper process) |
| **COLD** (`warm_with_timeout` after idle) | ~10–20 MB (service only) |
| **WARM** (`always_warm` default) | ~80–200 MB (Piper + loaded medium model) |
| **ACTIVE** (job in progress) | +small burst (ring buffer, per-job `aplay`) |

**8 GB Pi 5:** WARM paging is acceptable alongside existing HomePi services.  
**4 GB Pi 5:** Prefer `warm_with_timeout` or keep paging disabled when not in use.

### CPU

| Scenario | CPU impact |
|----------|------------|
| Idle (WARM, lazy DAC) | ~0–1% — Piper process idle; no open ALSA handle or `aplay` |
| Short alert (≤ 80 chars, doorbell) | Moderate burst ~0.3–1 s during Piper inference |
| Long-form (up to 4000 chars) | Longer synthesis burst; streaming reduces time-to-first-word |
| Playback (`aplay`) | Negligible |
| HiFi `PAGE1` / `PAGE0` | Negligible |

Paging is isolated from PCM router and AirPlay orchestration but shares CPU with all other Pi processes. The main contention scenario is long-form paging during heavy AirPlay use.

### Budget enforcement (already in spec)

- Lazy DAC open/close — no playback resources while idle.
- PCM pipe — no tmpfs/SD WAV files for speech.
- `maxInstalledVoices: 2` — caps voice storage.
- `idlePolicy` — `always_warm` (latency) vs `warm_with_timeout` (RAM).
- Paging disabled → no Piper loaded.

### Install image requirements

- Ship one bundled medium voice under `/var/lib/homepi/paging/voices/`.
- Ship default chime under `/var/lib/homepi/paging/chimes/default.wav` (~150–250 ms).
- Install script must verify Piper ARM64 binary and report installed paging footprint.
- Do not bundle more than the default voice in the base image.

---

## Implementation Phases

### Phase 1 — Configuration Foundation

Implement:

- Database tables (config, voices, chimes, API key, jobs).
- API config endpoints (including `idlePolicy`, `idleWarmTimeoutMs`, `dacIdleCloseDelayMs`).
- Paging API key in Audio → Settings.
- UI config card (DAC read-only from USB).
- Bundled default voice + chime on install image.
- Paging readiness state without live paging.

Acceptance criteria:

- User can enable/disable paging.
- Primary Paging DAC shown from USB assignments.
- API key can be generated and used for auth.
- Config persists across restart.
- UI shows missing DAC state if unplugged.

---

### Phase 2 — Voice Catalog and Chimes

Implement:

- Voice catalog metadata loader.
- Installed voice DB state.
- Remote/static sample preview for catalog voices.
- Voice install/remove APIs.
- Default voice selection.
- Chime upload, preview, active chime selection.

Acceptance criteria:

- User can browse voices without downloading all of them.
- User can preview catalog voice sample without Pi storage impact.
- User can install a second voice and swap default.
- Max installed voice count is enforced.
- User can upload and preview custom chimes.

---

### Phase 3 — Warm TTS Worker + PCM Pipe + Lazy DAC

Implement:

- Piper worker manager with bundled voice preload (`always_warm` default).
- Resource lifecycle states (DISABLED / COLD / WARM / ACTIVE).
- Ring buffer + PCM pipe to per-job `aplay`.
- Lazy DAC open after `#PAGE1`; lazy close after `dacIdleCloseDelayMs`.
- Idle policy config (`always_warm`, `warm_with_timeout`).
- Local voice preview through Primary Paging DAC only (no HiFi PAGE).
- Streaming mode for text > `streamThresholdChars`.

Acceptance criteria:

- TTS worker loads default voice at startup when `always_warm`.
- No ALSA handle or `aplay` process while idle (WARM).
- DAC opens only during active jobs; closes within grace period after playback.
- Preview starts quickly after worker is warm.
- Preview audio plays only through Primary Paging DAC.
- No tmpfs WAV files for speech.
- `audio.paging.dac.opened` / `audio.paging.dac.closed` events emitted per job.

---

### Phase 4 — HiFi2 Page Sequence + Speak API

Implement:

- `*PAGE1` / `*PAGE0` in homepi-hifi-serial.
- `#PAGE1` confirmation gate before DAC output.
- `POST /api/audio/paging/speak` with API key auth.
- `POST /api/audio/paging/chime`.
- Parallel synthesis + PAGE ON.
- Failure cleanup path.
- Job table timing updates.
- `waitUntil` response modes.

Acceptance criteria:

- Shortcuts API call produces whole-house announcement.
- Zones switch (`#PAGE1`) before speech hits DAC.
- Page turns off after playback.
- Failure after page-on attempts to turn page off.
- `*PAGE2` is never used by the automated path.
- Busy requests return `409 paging_busy`.
- Alert-class P95 latency meets design targets in timing logs.

---

### Phase 5 — UI Polish and Production Hardening

Implement:

- Event-driven status updates in UI (including resourceState, dacOpen).
- Idle policy toggle in Paging card (power saving).
- Whole-house preview confirmation.
- Clear readiness warnings.
- Last job status card.
- Robust timeout/error handling.
- Service health integration.
- Job retention pruning.

Acceptance criteria:

- UI accurately shows ready/not-ready state and WARM/COLD/ACTIVE.
- UI shows DAC connected vs DAC open separately.
- User can identify why paging is unavailable.
- Logs and events are enough to debug failures.
- Paging does not leave HiFi2 stuck in page mode after normal failures.
- Idle CPU near zero when paging disabled; minimal when WARM with DAC closed.

---

## Cursor Implementation Notes

### TypeScript Style

Use:

- ES modules.
- `async/await`.
- Strong request/response schemas.
- Explicit state machine types.
- Structured logs at every lifecycle stage.
- No silent catches.
- No hardcoded ALSA card numbers.
- No direct serial writes from API routes.

### Suggested Types

```ts
export type PagingState =
  | 'DISABLED'
  | 'NOT_READY'
  | 'READY'
  | 'BUSY'
  | 'TTS_GENERATING'
  | 'PAGE_STARTING'
  | 'PLAYING'
  | 'PAGE_ENDING'
  | 'FAILED';

export type PagingOnBusy = 'reject';

export type PagingWaitUntil = 'accepted' | 'playback_started';

export type PagingIdlePolicy = 'always_warm' | 'warm_with_timeout';

export type PagingResourceState = 'DISABLED' | 'COLD' | 'WARM' | 'ACTIVE';

export interface PagingSpeakRequest {
  text: string;
  voiceId?: string | null;
  source?: 'shortcuts' | 'api' | 'automation' | 'system' | 'preview';
  includeChime?: boolean;
  chimeId?: string | null;
  onBusy?: PagingOnBusy;
  waitUntil?: PagingWaitUntil;
}

export interface PagingChimeRequest {
  chimeId?: string;
  source?: 'shortcuts' | 'api' | 'automation' | 'system';
  onBusy?: PagingOnBusy;
  waitUntil?: PagingWaitUntil;
}

export interface PagingConfig {
  enabled: boolean;
  defaultVoiceId: string | null;
  activeVoiceId: string | null;
  activeChimeId: string;
  maxInstalledVoices: number;
  maxTextLength: number;
  maxPreviewTextLength: number;
  streamThresholdChars: number;
  minPrerollMs: number;
  defaultOnBusy: PagingOnBusy;
  idlePolicy: PagingIdlePolicy;
  idleWarmTimeoutMs: number;
  dacIdleCloseDelayMs: number;
  keepLastAudioForDebug: boolean;
}

export interface PagingReadiness {
  ready: boolean;
  state: PagingState;
  resourceState: PagingResourceState;
  enabled: boolean;
  pagingDacConfigured: boolean;
  pagingDacConnected: boolean;
  dacOpen: boolean;
  defaultVoiceInstalled: boolean;
  ttsWorkerReady: boolean;
  hifiConnected: boolean;
  busy: boolean;
  reason?: string;
}
```

### Suggested Classes

```txt
PagingOrchestrator
PagingConfigStore
PagingJobStore
PiperWorkerManager
PagingPcmPlayer
PagingChimePlayer
PagingResourceManager
HifiPagingController
PagingVoiceCatalog
PagingReadinessService
```

---

## Non-Goals for MVP

Do not implement these in the first version:

- Multiple simultaneous pages.
- Queueing pages (`onBusy: queue`).
- Replacing or ignoring busy requests (`onBusy: replace`, `ignore`).
- Dedupe keys / debounce windows.
- Cloud TTS dependency.
- Storing raw page text history.
- Downloading all voices.
- Using `*PAGE2` toggle for automation.
- Per-zone selective paging outside native HiFi2 behavior.
- Duplicate paging DAC configuration outside USB assignments.
- tmpfs WAV files for generated speech (pipe PCM instead).
- Long-lived `aplay` or open ALSA handle on paging DAC while idle.
- A separate HTTP API service for paging (use `apps/backend` + `homepi-audio-paging`).

---

## Final Target Behavior

When the system is fully implemented:

```txt
1. User assigns Primary Paging DAC in Audio/USB configuration.
2. Bundled default voice and chime are preinstalled; Piper stays WARM while paging enabled (default policy).
3. User generates paging API key in Audio → Settings for Shortcuts.
4. User configures page volume per zone in zone settings.
5. While idle: no ALSA handle open, no aplay running; minimal CPU.
6. iPhone Shortcut POSTs text to /api/audio/paging/speak.
7. HomePi validates API key and readiness (WARM, DAC connected but closed).
8. In parallel: Piper fills ring buffer; HomePi sends *PAGE1.
9. After #PAGE1 confirmed: lazy-open DAC, pipe speech PCM.
10. HiFi2 broadcasts source 8 to all zones at configured page volume.
11. After playback drains: lazy-close DAC within dacIdleCloseDelayMs.
12. HomePi ends page mode using *PAGE0.
13. HomePi emits events and logs the full sequence with timing metrics.
14. No speech audio files are written to disk.
```

Optional chime-only path:

```txt
POST /api/audio/paging/chime -> *PAGE1 -> chime WAV -> *PAGE0
```

This design keeps the feature fast, lightweight, reliable, storage-conscious, and aligned with HomePi's event-driven architecture.
