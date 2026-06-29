# HomePi Sockets and Broker

## Canonical layout

```text
/run/homepi/
  broker/broker.sock          homepi-broker (optional event fanout)
  health/health.sock          homepi-health (system/module health)
  audio/
    audio.sock                homepi-audio (module facade)
    hifi-serial.sock          internal
    pcm-router.sock           internal
    metadata.sock             internal
    audio-realtime.sock       internal
    paging.sock               internal
  sensors/
    sensors.sock              homepi-sensors (module facade)
  usb/
    usb.sock                  homepi-usb-devices
```

## Socket permissions

```text
owner: service-specific user
group: homepi
mode: 0660
```

## Broker (`homepi-broker`)

Socket: `/run/homepi/broker/broker.sock`

Allowed: `publish`, `subscribe`, `unsubscribe`, `snapshot`, `ping`

Not allowed: command routing, hardware control, module state ownership

Topic format: `homepi.<module>.<entity>.<event>`

## Backend HTTP (loopback)

| Port | Path | Protocol |
|------|------|----------|
| 3000 | `/api/health` | HTTP — proxies homepi-health |
| 3000 | `/api/core/status` | HTTP — hierarchical module health |
| 3000 | `/events`, `/api/events` | SSE |
| 3000 | `/ws`, `/api/ws` | WebSocket |

Production: NGINX proxies to `127.0.0.1:3000`.

## Legacy flat paths (removed in Phase 10)

See [`legacy-decommission.md`](legacy-decommission.md).
