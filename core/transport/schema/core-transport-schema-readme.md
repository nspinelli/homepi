# HomePi Core Transport Schemas

## Purpose

This folder contains shared JSON schemas for structured transport data.

Schemas define:
- message envelopes
- transport errors
- connection lifecycle events
- transport configuration
- NDJSON frame expectations

## Files

- `transport-envelope.schema.json`
- `transport-error.schema.json`
- `connection-event.schema.json`
- `unix-socket-config.schema.json`
- `pipe-config.schema.json`
- `websocket-config.schema.json`
- `sse-config.schema.json`
- `ndjson-frame.schema.json`

## Rule

Schemas in this folder define shared transport infrastructure only.

Module-specific payload schemas belong with the owning module.

Example:

```text
modules/audio/hifi-serial/schema/hifi-message.schema.json
```

not:

```text
core/transport/schema/hifi-message.schema.json
```
