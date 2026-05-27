# HomePi Core Transport TypeScript

## Purpose

The TypeScript transport implementation provides shared helpers for Node.js services and backend applications.

It should provide:
- Unix socket client/server helpers
- WebSocket helper interfaces
- SSE helper interfaces
- NDJSON encoder/decoder
- transport envelope types
- reconnect helpers
- backpressure helpers
- transport error helpers

## Expected Source Layout

```text
core/transport/ts/
├─ core-transport-ts-readme.md
├─ package.json
└─ src/
   ├─ index.ts
   ├─ envelope.ts
   ├─ errors.ts
   ├─ ndjson.ts
   ├─ reconnect-policy.ts
   ├─ backpressure.ts
   ├─ unix-socket-client.ts
   ├─ unix-socket-server.ts
   ├─ sse.ts
   └─ websocket.ts
```

## Rules

TypeScript services MUST use this package rather than inventing local transport formats.
