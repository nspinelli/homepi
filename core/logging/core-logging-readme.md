# HomePi Core Logging

## Purpose

The HomePi logging system provides a unified, structured, production-grade logging framework for all HomePi services and modules.

Logging is considered a core system primitive and is required for:
- observability
- debugging
- AI-assisted diagnostics
- runtime traceability
- lifecycle monitoring
- watchdog integration
- operational visibility
- event reconstruction

All HomePi services MUST use the shared logging system.

---

# Philosophy

HomePi logging is designed around the following principles:

## Structured First

All logs MUST be structured JSON.

Free-form string logs are forbidden.

This allows:
- machine parsing
- AI analysis
- reliable filtering
- event correlation
- long-term maintainability

---

## Journald Native

HomePi uses:
- stdout
- stderr
- systemd
- journald
- journalctl

as the primary logging infrastructure.

Services MUST emit logs to stdout/stderr only.

systemd/journald is responsible for:
- persistence
- indexing
- filtering
- retention
- transport
- querying

---

## Unified System Logging

All HomePi services write logs into the same journald runtime.

This creates a unified, centralized, queryable application-wide event stream without requiring a custom logging daemon.

Every HomePi service:
- emits structured JSON logs
- writes logs to stdout/stderr
- runs under systemd
- is collected automatically by journald

This architecture provides:
- centralized observability
- cross-service debugging
- AI-assisted tracing
- event reconstruction
- service isolation
- operational simplicity

---

## AI-Debuggable

Logs must be designed so an AI agent can:
- reconstruct execution flow
- identify failures
- trace operations
- understand runtime state
- identify dependencies
- detect abnormal transitions

Logs are not only for humans.

Logs are system telemetry.

---

## Event-Oriented

Logs should describe:
- meaningful state transitions
- lifecycle changes
- message flow
- dependency changes
- runtime decisions
- error conditions

Avoid meaningless spam.

Good logs describe:
- what happened
- why it happened
- what changed
- what the system did next

---

# Logging Architecture

HomePi does NOT use a centralized logging daemon.

There is no:
- homepi-core-logger.service
- custom log collector
- shared logging socket
- centralized logging queue

Instead:

Service
→ stdout/stderr
→ systemd
→ journald
→ journalctl

The shared `core/logging` module is a logging framework and contract used by all services.

It is NOT a standalone logging service.

---

# Responsibilities

The logging system is responsible for:

- structured JSON output
- log level handling
- correlation IDs
- event naming
- journald compatibility
- debug tracing
- rate limiting
- watchdog telemetry
- transport-safe logging
- cross-language consistency
- timestamp generation

---

# Non-Responsibilities

The logging system is NOT responsible for:

- business logic
- metrics aggregation
- analytics pipelines
- monitoring dashboards
- alerting systems
- database persistence
- protocol parsing

---

# Directory Structure

```text
core/logging/
├─ README.md
├─ schema/
│  ├─ log-message.schema.json
│  ├─ event-registry.json
│  └─ level-registry.json
│
├─ cpp/
│  ├─ log.hpp
│  ├─ log_rate_limiter.hpp
│  └─ log_watchdog.hpp
│
├─ ts/
│  ├─ logger.ts
│  ├─ log-types.ts
│  └─ correlation.ts
│
├─ examples/
│  ├─ cpp-service-example.md
│  └─ ts-service-example.md
│
└─ rules/
   └─ logging-rules.md
```

---

# Service Naming Rules

All HomePi services MUST use consistent systemd service naming.

Required format:

```text
homepi-<service-name>
```

Examples:
- homepi-hifi-serial
- homepi-contact-sensors
- homepi-backend
- homepi-tts

This enables:
- predictable journalctl filtering
- consistent observability
- easier AI diagnostics
- runtime grouping

---

# Logging Levels

## DEBUG

Used for:
- detailed execution tracing
- protocol parsing
- internal state transitions
- timing diagnostics
- AI debugging visibility

DEBUG logs may be verbose.

DEBUG logs SHOULD contain:
- raw payloads
- parsed values
- timing information
- state snapshots
- internal decisions

---

## INFO

Used for:
- lifecycle events
- successful operations
- service state changes
- normal runtime activity

INFO should represent the normal operational story of the service.

Examples:
- service_started
- device_connected
- config_loaded
- socket_created

---

## WARN

Used for:
- recoverable failures
- unexpected states
- degraded operation
- retry conditions

WARN indicates something abnormal occurred but the system remains functional.

---

## ERROR

Used for:
- operation failures
- unrecoverable issues
- dependency failures
- invalid states
- crashes
- protocol violations

ERROR logs should always provide enough information to diagnose the failure.

---

# Log Schema

All logs MUST follow the shared schema.

## Required Fields

```json
{
  "ts": "2026-05-26T22:15:44.123Z",
  "service": "homepi-hifi-serial",
  "module": "core.transport",
  "level": "INFO",
  "event": "socket_created",
  "correlationId": "socket-create-001",
  "message": "Unix socket created",
  "data": {}
}
```

---

# Required Metadata

Every log entry MUST include:

- ts
- service
- module
- level
- event
- correlationId
- message
- data

These fields are mandatory across:
- C++
- TypeScript
- all services
- all modules

---

# Example Completeness Rules

All full log examples in this document MUST include every required metadata field.

Required fields:
- ts
- service
- module
- level
- event
- correlationId
- message
- data

Examples may omit fields ONLY if explicitly labeled as:
- partial payload
- abbreviated example
- schema fragment

This rule exists to:
- prevent schema drift
- reinforce consistency
- improve AI training context
- reduce implementation ambiguity
- ensure copy/paste-safe examples

---

## Correct Full Example

```json
{
  "ts": "2026-05-26T22:15:44.123Z",
  "service": "homepi-example-service",
  "module": "core.logging",
  "level": "DEBUG",
  "event": "message_parsed",
  "correlationId": "cmd-001",
  "message": "Message parsed successfully",
  "data": {
    "rawInput": "#Z4POWER1",
    "parsedType": "zone_power",
    "zone": 4,
    "power": true,
    "durationMs": 2
  }
}
```

---

## Incorrect Example

```json
{
  "level": "DEBUG",
  "event": "message_parsed",
  "message": "Parsed message"
}
```

The above example is invalid because it omits required metadata fields.

---

# Schema Field Definitions

| Field | Description |
|---|---|
| ts | ISO8601 UTC timestamp |
| service | systemd service name |
| module | logical module name |
| level | DEBUG / INFO / WARN / ERROR |
| event | stable event identifier |
| correlationId | operation trace identifier |
| message | human-readable summary |
| data | structured event-specific payload |

---

# Correlation IDs

## Purpose

Correlation IDs allow reconstruction of a single logical operation across multiple log entries.

This is critical for:
- AI debugging
- distributed tracing
- protocol debugging
- async operations
- socket communication
- serial communication

---

## Rules

A correlation ID MUST:
- represent one logical operation
- remain stable throughout the operation
- never be reused across unrelated operations

---

## Example

```text
SendCommand:#Z4POWER1
├─ command_queued
├─ command_sent
├─ serial_line_received
├─ response_parsed
├─ state_updated
└─ client_notified
```

All entries share the same correlationId.

---

# Event Naming

Events MUST:
- use snake_case
- remain stable over time
- describe state transitions
- avoid ambiguous wording

---

## Good Examples

```text
service_started
socket_created
client_connected
serial_line_received
message_parsed
device_disconnected
watchdog_timeout
config_loaded
```

---

## Bad Examples

```text
started
done
thing_happened
received
event
update
```

---

# Canonical Event Registry

All shared event names MUST eventually be registered centrally.

Location:

```text
core/logging/schema/event-registry.json
```

The registry exists to prevent:
- duplicate meanings
- inconsistent naming
- ambiguous lifecycle events
- event drift
- module fragmentation

---

## Registry Rules

The registry MUST:
- define canonical event names
- define event descriptions
- define recommended log levels
- remain backwards compatible
- be shared across all services

---

## Example Registry Entry

```json
{
  "service_started": {
    "level": "INFO",
    "description": "Service lifecycle startup completed"
  },
  "client_connected": {
    "level": "INFO",
    "description": "Client connection established"
  },
  "watchdog_timeout": {
    "level": "ERROR",
    "description": "Watchdog timeout detected"
  }
}
```

---

# Debug Logging

DEBUG mode exists specifically to improve:
- runtime traceability
- protocol debugging
- AI-assisted diagnostics

DEBUG logs SHOULD contain:
- raw payloads
- parsed values
- internal routing decisions
- execution timing
- retry logic
- queue behavior

---

## Example DEBUG Log

```json
{
  "ts": "2026-05-26T22:15:44.123Z",
  "service": "homepi-example-service",
  "module": "core.logging",
  "level": "DEBUG",
  "event": "message_parsed",
  "correlationId": "cmd-001",
  "message": "Message parsed successfully",
  "data": {
    "rawInput": "#Z4POWER1",
    "parsedType": "zone_power",
    "zone": 4,
    "power": true,
    "durationMs": 2
  }
}
```

---

# Journald Integration

All services MUST:
- log to stdout/stderr only
- run under systemd
- use journald

---

# Example System Queries

## Show All HomePi Logs

```bash
journalctl | grep homepi-
```

---

## Follow One Service

```bash
journalctl -u homepi-hifi-serial -f
```

---

## Show Structured JSON Logs

```bash
journalctl -u homepi-hifi-serial -o cat
```

---

## Filter ERROR Logs

```bash
journalctl -o cat | jq 'select(.level=="ERROR")'
```

---

## Filter By Correlation ID

```bash
journalctl -o cat | jq 'select(.correlationId=="cmd-001")'
```

---

## Filter By Module

```bash
journalctl -o cat | jq 'select(.module=="core.transport")'
```

---

# Example Cross-Service Trace

A single user action may traverse multiple services.

Example:

Frontend
↓
Backend WebSocket
↓
Audio Module
↓
Transport Layer
↓
Serial Service
↓
HiFi2 Controller
↓
State Broadcast
↓
Frontend Update

All related logs should share the same correlationId.

This allows:
- end-to-end tracing
- AI-assisted debugging
- runtime reconstruction
- latency analysis

---

# JSON Requirements

All logs MUST:
- be valid JSON
- be single-line
- be UTF-8 safe
- escape all user-controlled strings
- avoid malformed output

The logging framework MUST safely escape:
- quotes
- backslashes
- control characters
- newlines

Malformed JSON logs are considered a logging failure.

---

# Rate Limiting

The logging system MUST support rate limiting.

This prevents:
- log flooding
- journald overload
- storage exhaustion
- repeated error spam

Rate limiting is especially important for:
- reconnect loops
- hardware failures
- serial parsing failures
- watchdog failures

---

# Watchdog Logging

The logging system integrates with:
- systemd watchdog
- internal watchdog telemetry

Watchdog events MUST be logged.

Examples:
- watchdog_ping
- watchdog_timeout
- watchdog_recovered

---

# Cross-Language Consistency

C++ and TypeScript implementations MUST:
- produce identical schemas
- use identical levels
- use identical event naming
- use identical timestamp formatting

This ensures:
- consistent querying
- shared tooling
- AI compatibility

---

# AI Observability Requirements

Logs must be designed for machine reasoning.

A log should allow an AI agent to determine:
- what occurred
- why it occurred
- what changed
- what the system decided
- what happened next

Logs should expose:
- dependency state
- queue state
- timing
- retries
- protocol parsing
- transport transitions
- connection lifecycle

DEBUG mode should maximize runtime traceability.

---

# Crash Safety

Services MUST continue operating if:
- journald restarts
- stdout blocks temporarily
- another service crashes

A failure in one HomePi service must never prevent another service from logging.

This is one of the primary reasons HomePi avoids a centralized logging daemon.

---

# Log Stability Rules

The following should remain stable over time:
- log schema
- event names
- log levels
- timestamp formatting
- correlation behavior

Stability is critical for:
- AI tooling
- automated debugging
- long-term maintainability
- operational consistency

---

# Forbidden Practices

The following are forbidden:

## Raw Console Logging

```cpp
std::cout << "connected";
```

```ts
console.log("connected");
```

---

## Multi-Line Logs

Logs MUST remain single-line JSON.

---

## Unstructured Strings

```text
device failed lol
```

---

## Hidden Failures

Errors MUST be logged.

Silent failures are forbidden.

---

# Logging Rules

## Required Logging

Services MUST log:
- startup
- shutdown
- config load
- dependency readiness
- socket creation
- connection lifecycle
- device lifecycle
- errors
- retries
- watchdog state
- message parsing
- state transitions

---

## Logging Intent

Logs should answer:

```text
What happened?
Why did it happen?
What changed?
What happened next?
```

---

# Performance Rules

Logging must:
- avoid blocking operations
- avoid heap-heavy formatting
- avoid excessive allocations
- support high-frequency services
- remain safe under load

---

# Documentation Rules

All schema examples throughout HomePi documentation should be fully valid unless explicitly marked otherwise.

Documentation examples are considered part of the architecture contract.

Examples should be:
- copy/paste safe
- schema compliant
- implementation accurate
- AI-readable
- operationally realistic

---

# Future Expansion

Future enhancements may include:
- OpenTelemetry bridges
- remote log streaming
- structured metrics
- distributed tracing
- centralized dashboards
- log replay tooling

These systems must remain compatible with the core schema.

---

# Final Rule

Logging is not optional in HomePi.

Logging is a core architectural contract.

Every service, module, transport layer, runtime component, and hardware integration must produce structured, traceable, AI-readable logs through the shared HomePi logging system.