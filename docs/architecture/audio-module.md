# HomePi Audio Module

## Client-facing facade

**Service:** `homepi-audio`  
**Socket:** `/run/homepi/audio/audio.sock`  
**Icon:** `/audio-controller.png`

The backend routes all audio HTTP commands through the facade — not internal service sockets.

## Internal services

| Service | Socket | Role |
|---------|--------|------|
| `homepi-hifi-serial` | `hifi-serial.sock` | Hi-Fi2 serial controller |
| `homepi-pcm-router` | `pcm-router.sock` | PCM routing to DAC |
| `homepi-metadata` | `metadata.sock` | Shairport metadata |
| `homepi-audio-paging` | `paging.sock` | Paging output |
| `homepi-nqptp` | — | AirPlay PTP timing |
| `homepi-shairport-supervisor` | — | Shairport lifecycle |
| `homepi-shairport@N` | — | Per-zone AirPlay |
| `homepi-usb-devices` | `/run/homepi/usb/usb.sock` | USB assignments (supports audio) |

## Capabilities (independent degraded state)

| Capability ID | Display name |
|---------------|--------------|
| `zone-control` | Zone Control |
| `airplay` | AirPlay |
| `pcm-routing` | PCM Routing |
| `paging` | Paging |

## Events

Normalized broker topics:

```text
homepi.audio.zone.changed
homepi.audio.capability.changed
homepi.audio.paging.started
homepi.audio.paging.failed
```

## Failure isolation

PCM router failure must not stop Hi-Fi2 zone control. Paging failure must not stop normal zone control.
