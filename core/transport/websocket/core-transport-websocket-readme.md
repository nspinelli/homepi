# HomePi Core Transport WebSocket

## Purpose

WebSocket is the preferred browser-facing bidirectional transport.

## Use Cases

- UI command channels
- real-time controls
- client/server state synchronization
- interactive module control

## Rules

- WebSocket payloads SHOULD use the shared transport envelope.
- Connection lifecycle MUST be logged.
- Client messages MUST be validated before execution.
- Backpressure MUST be handled.
- Authentication/authorization belongs to core/auth or the owning backend service.
