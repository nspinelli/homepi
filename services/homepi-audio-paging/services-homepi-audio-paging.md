# homepi-audio-paging

Native paging service for HomePi voice announcements and chimes.

## Responsibilities

- Subscribes to `audio.paging.command.*` broker commands.
- Publishes `audio.paging.*` readiness, resource, and job lifecycle events.
- Resolves paging DAC assignment from `homepi-usb-devices` (`usb_assignments.paging`) over Unix socket.
- Uses `plug:AudioPaging` for playback with lazy open/close behavior.
- Manages Piper TTS warm/cold lifecycle (`always_warm` default).
- Exposes Unix socket API for status/config/voice/chime operations.

## Runtime Paths

- Service socket: `/run/homepi/audio-paging.sock`
- Events broker socket: `/run/homepi/events.sock`
- USB devices socket: `/run/homepi/usb-devices.sock`
- SQLite DB: `/opt/homepi/runtime/state/homepi.sqlite`
- Bundled voice target: `/var/lib/homepi/paging/voices/en_US-lessac-medium.onnx`
- Default chime target: `/var/lib/homepi/paging/chimes/default.wav`

## Build

```bash
cmake -S services/homepi-audio-paging -B services/homepi-audio-paging/build
cmake --build services/homepi-audio-paging/build --parallel
```

## Install

```bash
sudo bash services/homepi-audio-paging/scripts/install.sh
```
