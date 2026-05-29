# homepi-pcm-router

Routes one of 16 ALSA loopback PCM zones to the **Primary Audio Output** USB DAC configured in HomePi settings.

## Prerequisites

- `homepi-usb-devices` with Primary Audio Output assigned
- Stable ALSA card `HomePiPrimaryAudio` (deploy modprobe via USB settings save)
- Mosquitto broker
- Kernel module `snd-aloop` with cards `HomePiZonesA` and `HomePiZonesB`

## Install

```bash
sudo bash services/homepi-pcm-router/scripts/install.sh
```

## ALSA loopback

Install writes:

- `/etc/modules-load.d/homepi-aloop.conf`
- `/etc/modprobe.d/homepi-aloop.conf`

Validate:

```bash
/opt/homepi/services/pcm-router/bin/homepi-pcm-router --validate-alsa
```

## Primary DAC

The router reads `usb_assignments.audio_primary_device_id` from `/opt/homepi/runtime/state/homepi.sqlite` at startup and opens `hw:HomePiPrimaryAudio,0`.

After changing Primary Audio Output in the UI, save assignments — the post-assignment hook rebinds the DAC and reboots if needed, then restarts this service. Manual replug is usually not required.

## Future Shairport zones

See `config/shairport-zone-template.conf`. The separate `homepi-shairport` service will write playback to `hw:HomePiZonesA|B,0,<substream>` and publish MQTT under `shairport/zone/<1-16>/`.

## Socket events

NDJSON core event envelopes on `/run/homepi/pcm-router.sock` (subscribe RPC). Backend bridges to SSE.
