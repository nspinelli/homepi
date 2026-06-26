# HomePi Sockets and Ports

## Backend HTTP (loopback)

| Port | Path | Protocol |
|------|------|----------|
| 3000 | `/api/health` | HTTP |
| 3000 | `/api/runtime/status` | HTTP |
| 3000 | `/api/core/status` | HTTP |
| 3000 | `/api/usb-devices/*` | HTTP |
| 3000 | `/api/hifi-serial/*` | HTTP |
| 3000 | `/api/audio/paging/*` | HTTP |
| 3000 | `/events`, `/api/events` | SSE |
| 3000 | `/ws`, `/api/ws` | WebSocket |

Production: NGINX proxies these routes to `127.0.0.1:3000` at `http://homepi.local`.

## Native Unix sockets

Default directory: `/run/homepi` (from `runtime.paths.socketDir` in service config).

| Socket file | Service | Backend consumer |
|-------------|---------|------------------|
| `events.sock` | `homepi-events` | `EventsBrokerBridge`, native service clients |
| `usb-devices.sock` | `homepi-usb-devices` | `UsbDevicesClient`, `UsbDevicesEventBridge` |
| `hifi-serial.sock` | `homepi-hifi-serial` | `HifiSerialClient`, `HifiSerialEventBridge` |
| `pcm-router.sock` | `homepi-pcm-router` | `PcmRouterClient`, `PcmRouterEventBridge` |
| `metadata.sock` | `homepi-metadata` | `MetadataClient`, `MetadataEventBridge` |
| `audio-realtime.sock` | `homepi-metadata` | `AudioRealtimeBridge` (latest-value progress) |
| `audio-paging.sock` | `homepi-audio-paging` | `PagingClient` |

### Protocol

Most control sockets use NDJSON over Unix stream sockets. RPC requests include `method` and `correlationId`. Event subscribers send:

```json
{"method":"subscribe","correlationId":"…"}
```

`audio-realtime.sock` uses a latest-value frame protocol (see `docs/homepi-update.md`).

## Services without primary control socket

| Service | Observability |
|---------|---------------|
| `homepi-nqptp` | journald + systemd fallback |
| `homepi-shairport-supervisor` | journald + systemd fallback |
| `homepi-audio-orchestrator` | `core/events` topics + journald |
| `homepi-shairport@N` | Per-zone Shairport process (MQTT remote to Mosquitto) |

Legacy `homepi-metadata@N` per-zone units are disabled on install; metadata is a single `homepi-metadata.service`.

## Other ports (reference)

| Port | Service | Notes |
|------|---------|-------|
| 319–320 UDP | nqptp | AirPlay PTP (upstream binary) |
| 5000–5003 TCP | shairport-sync zones | Per-zone AirPlay |
| 1883 TCP | mosquitto | Loopback MQTT (Shairport remote control) |
| 22 TCP | ssh | Remote administration (independent of HomePi boot order) |
