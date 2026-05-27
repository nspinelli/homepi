# HomePi Core Transport

## Purpose

The HomePi transport system defines the shared communication primitives used by HomePi services, modules, applications, and runtime components.

Transport is considered a core architectural primitive.

The transport system exists to provide:
- reliable inter-process communication
- consistent message framing
- typed transport envelopes
- Unix domain socket standards
- named pipe standards
- WebSocket standards
- Server-Sent Events standards
- reconnect behavior
- connection lifecycle rules
- backpressure expectations
- structured transport logging
- AI-readable communication contracts

All HomePi services and modules that communicate across process boundaries MUST use the shared transport standards.

---

# Philosophy

HomePi transport is designed around the following principles:

## Contracts First

Transport behavior MUST be defined by stable contracts.

Transport contracts include:
- connection lifecycle
- message envelope shape
- framing rules
- error payloads
- reconnect behavior
- logging events
- shutdown behavior

---

## Local IPC First

HomePi runs primarily on a Raspberry Pi.

Local service-to-service communication SHOULD prefer:
- Unix domain sockets
- named pipes
- NDJSON framing

when communication remains local to the device.

---

## Network Transports At The Edge

Network-facing transports such as:
- HTTP
- WebSocket
- SSE

SHOULD generally live at the application/backend boundary.

Core transport provides shared rules and helpers.

Services/modules decide which transports they expose.

---

## Module-Owned Message Meaning

Core transport defines HOW messages move.

Modules define WHAT messages mean.

Core transport MUST NOT contain:
- audio command meanings
- sensor event meanings
- HiFi protocol parsing
- TTS business logic
- automation behavior

---

## Observable Communication

Every meaningful transport state transition MUST be logged using `core/logging`.

Transport logs should allow an AI agent to reconstruct:
- connection flow
- message delivery
- framing issues
- client lifecycle
- retry behavior
- backpressure conditions
- disconnect causes

---

# Responsibilities

The core transport system is responsible for:

- transport standards
- shared message envelopes
- Unix socket conventions
- named pipe conventions
- WebSocket conventions
- SSE conventions
- NDJSON framing rules
- connection lifecycle rules
- reconnect behavior
- structured transport errors
- backpressure rules
- transport logging standards
- cross-language consistency

---

# Non-Responsibilities

The core transport system is NOT responsible for:

- module business logic
- hardware protocols
- application routing decisions
- database persistence
- frontend rendering
- authorization policy
- module-specific message schemas
- device-specific parsing

---

# Directory Structure

```text
core/transport/
├─ core-transport-readme.md
│
├─ schema/
│  ├─ core-transport-schema-readme.md
│  ├─ transport-envelope.schema.json
│  ├─ transport-error.schema.json
│  ├─ connection-event.schema.json
│  ├─ unix-socket-config.schema.json
│  ├─ pipe-config.schema.json
│  ├─ websocket-config.schema.json
│  ├─ sse-config.schema.json
│  └─ ndjson-frame.schema.json
│
├─ ts/
│  ├─ core-transport-ts-readme.md
│  ├─ package.json
│  └─ src/
│
├─ cpp/
│  ├─ core-transport-cpp-readme.md
│  ├─ CMakeLists.txt
│  └─ include/
│
├─ unix-sockets/
│  └─ core-transport-unix-sockets-readme.md
│
├─ pipes/
│  └─ core-transport-pipes-readme.md
│
├─ websocket/
│  └─ core-transport-websocket-readme.md
│
├─ sse/
│  └─ core-transport-sse-readme.md
│
├─ ndjson/
│  └─ core-transport-ndjson-readme.md
│
├─ examples/
│  ├─ transport-envelope.example.json
│  ├─ transport-error.example.json
│  ├─ connection-event.example.json
│  ├─ unix-socket-config.example.json
│  ├─ pipe-config.example.json
│  ├─ websocket-config.example.json
│  ├─ sse-config.example.json
│  └─ ndjson-stream.example.ndjson
│
└─ rules/
   └─ core-transport-rules-readme.md
```

---

# Transport Types

## Unix Domain Sockets

Preferred for local service-to-service communication.

Examples:
- backend to native daemon
- module manager to service
- local control channels
- local event streams

Standard path:

```text
/run/homepi/<service>.sock
```

---

## Named Pipes

Used for simple one-way stream integration when required by external tools.

Examples:
- metadata pipes
- audio/control hooks
- legacy process integration

Named pipes MUST NOT be used as a general replacement for bidirectional sockets.

---

## NDJSON

Preferred framing format for local stream messages.

Each message is:
- one JSON object
- one line
- UTF-8
- newline-delimited
- schema-valid

---

## WebSocket

Used for bidirectional browser/backend communication.

Examples:
- real-time UI controls
- frontend command channels
- state update streams requiring client commands

---

## SSE

Used for one-way backend-to-browser event streams.

Examples:
- dashboard state updates
- health events
- logs/event feeds
- read-only live updates

---

# Core Rule

Core transport defines HOW communication works.

Modules and services define WHAT messages mean.

---

# Message Envelope

All transport messages SHOULD use the shared envelope unless a lower-level protocol requires otherwise.

Envelope shape:

```json
{
  "version": 1,
  "id": "msg-001",
  "type": "event",
  "source": "homepi-hifi-serial",
  "topic": "modules.audio.zone",
  "correlationId": "cmd-001",
  "timestamp": "2026-05-27T16:00:00.000Z",
  "payload": {}
}
```

---

# Connection Lifecycle

Transport lifecycle events:

```text
connection_opening
connection_opened
connection_authenticated
connection_ready
message_received
message_sent
message_dropped
backpressure_detected
connection_closing
connection_closed
connection_error
reconnect_scheduled
```

---

# Logging Requirements

Transport systems MUST emit structured logs for:
- socket_created
- socket_closed
- client_connected
- client_disconnected
- message_received
- message_sent
- message_parse_failed
- backpressure_detected
- reconnect_scheduled
- transport_error

All log entries MUST follow `core/logging`.

---

# Error Handling

Transport errors MUST be structured.

Errors should include:
- code
- message
- retryable
- transport
- connectionId when available
- correlationId when available

---

# Backpressure

Transport implementations MUST define how they handle slow consumers.

Allowed strategies:
- queue with limit
- drop oldest
- drop newest
- disconnect slow client
- reject write

The selected strategy MUST be documented by the owning service/module.

---

# Reconnect Rules

Clients SHOULD use bounded reconnect behavior.

Reconnect behavior MUST:
- avoid retry storms
- support backoff
- emit structured logs
- stop cleanly during shutdown

---

# Cross-Language Consistency

Transport behavior MUST remain consistent across:
- TypeScript
- C++
- runtime scripts
- module services

This includes:
- envelope shape
- framing
- error structure
- lifecycle event names
- logging behavior

---

# AI Requirements

Transport systems must remain AI-readable.

AI agents should be able to determine:
- what connection opened
- what message was sent
- what message failed
- why a client disconnected
- whether a transport issue is retryable
- where backpressure occurred

---

# Forbidden Practices

## Unframed Streams

Streams MUST define framing.

---

## Silent Disconnects

Disconnects MUST be logged.

---

## Free-Form Payloads Without Envelope

Core transport messages SHOULD use the shared envelope unless explicitly documented.

---

## Module Logic In Core Transport

Core transport MUST NOT parse module-specific commands.

---

## Infinite Reconnect Storms

Reconnect loops MUST be bounded or backoff-controlled.

---

# Documentation Rules

All examples must be:
- schema compliant
- copy/paste safe
- implementation realistic
- AI-readable
- operationally useful

---

# Final Rule

Transport is not optional in HomePi.

Every cross-process communication path MUST use documented HomePi transport standards, structured logging, deterministic lifecycle behavior, and schema-valid message contracts where applicable.
