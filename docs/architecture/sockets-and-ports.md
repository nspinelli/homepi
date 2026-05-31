# HomePi Sockets and Ports

## Backend HTTP (loopback)

| Port | Path | Protocol |
|------|------|----------|
| 3000 | `/api/health` | HTTP |
| 3000 | `/api/runtime/status` | HTTP |
| 3000 | `/api/core/status` | HTTP |
| 3000 | `/events`, `/api/events` | SSE |
| 3000 | `/ws`, `/api/ws` | WebSocket |

Production: NGINX proxies these routes to `127.0.0.1:3000`.

## Native Unix sockets

Default directory: `/run/homepi` (from `runtime.paths.socketDir` in service config).

| Socket file | Service | Backend consumer |
|-------------|---------|------------------|
| `usb-devices.sock` | `homepi-usb-devices` | `UsbDevicesClient`, `UsbDevicesEventBridge` |
| `hifi-serial.sock` | `homepi-hifi-serial` | `HifiSerialClient`, `HifiSerialEventBridge` |
| `pcm-router.sock` | `homepi-pcm-router` | `PcmRouterClient`, `PcmRouterEventBridge` |

### Protocol

NDJSON over Unix stream sockets. RPC requests include `method` and `correlationId`. Event subscribers send:

```json
{"method":"subscribe","correlationId":"…"}
```

## Services without sockets

| Service | Observability |
|---------|---------------|
| `homepi-nqptp` | journald + systemd fallback |
| `homepi-metadata@N` | journald + systemd fallback |
| `homepi-shairport-supervisor` | journald + systemd fallback |

## Other ports (reference)

| Port | Service | Notes |
|------|---------|-------|
| 319–320 UDP | nqptp | AirPlay PTP (upstream binary) |
| 5000–5003 TCP | shairport-sync zones | Per-zone AirPlay |
