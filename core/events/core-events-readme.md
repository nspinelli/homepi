# HomePi Core Events

## Purpose

The HomePi event system defines the shared event model used across services, modules, applications, transports, and frontend synchronization.

Events are a core architectural primitive.

The event system exists to provide:
- stable event envelopes
- publish/subscribe semantics
- snapshot and delta rules
- event ordering expectations
- replay boundaries
- event naming conventions
- event validation
- AI-readable runtime behavior
- cross-service traceability

Core events define HOW events are represented and routed.

Modules define WHAT the events mean.

---

# Philosophy

## Event-Driven First

HomePi should prefer event-driven communication whenever possible.

Polling should be avoided unless a device or external dependency requires it.

## Stable Contracts

Event names, envelope shape, timestamps, source names, and correlation IDs MUST remain stable over time.

## Snapshot + Delta

Services that expose runtime state SHOULD support:
- an initial authoritative snapshot
- follow-up delta events

This is critical for frontend reconnects and AI-assisted state reconstruction.

## Module-Owned Meaning

`core/events` MUST NOT define audio, sensor, TTS, or device-specific event payload meanings.

---

# Responsibilities

`core/events` is responsible for:
- event envelope contracts
- event naming rules
- event lifecycle rules
- snapshot/delta standards
- subscription standards
- replay standards
- event validation
- event logging expectations

---

# Non-Responsibilities

`core/events` is NOT responsible for:
- module business logic
- transport implementation
- database persistence
- frontend rendering
- hardware protocol parsing

---

# Directory Structure

```text
core/events/
├─ core-events-readme.md
├─ schema/
│  ├─ core-events-schema-readme.md
│  ├─ event-envelope.schema.json
│  ├─ subscription.schema.json
│  ├─ snapshot.schema.json
│  ├─ event-error.schema.json
│  └─ replay-request.schema.json
├─ ts/
│  ├─ core-events-ts-readme.md
│  └─ src/
├─ cpp/
│  ├─ core-events-cpp-readme.md
│  └─ include/core/events/
├─ subscriptions/
│  └─ core-events-subscriptions-readme.md
├─ snapshots/
│  └─ core-events-snapshots-readme.md
├─ replay/
│  └─ core-events-replay-readme.md
├─ examples/
└─ rules/
   └─ core-events-rules-readme.md
```

---

# Required Event Envelope

All shared HomePi events SHOULD use the event envelope.

```json
{
  "version": 1,
  "id": "evt-001",
  "source": "homepi-hifi-serial",
  "topic": "modules.audio.zone",
  "event": "zone_power_changed",
  "correlationId": "cmd-001",
  "timestamp": "2026-05-27T16:00:00.000Z",
  "payload": {}
}
```

---

# Event Naming

Event names MUST:
- use snake_case
- describe what changed
- remain stable
- avoid vague names

Good:
- `service_started`
- `state_snapshot_emitted`
- `client_subscribed`
- `device_disconnected`

Bad:
- `update`
- `thing`
- `event`
- `changed`

---

# Logging Requirements

Event systems MUST log:
- event_published
- event_received
- event_rejected
- subscription_created
- subscription_removed
- snapshot_emitted
- replay_requested
- replay_failed


    ---

    # Cursor AI Implementation Rules

    Cursor AI agents MUST:
    - read this README before implementing `core/events`
    - follow the documented folder structure
    - preserve naming conventions
    - use `core/logging` for structured logs
    - use `core/config` for configuration
    - use `core/runtime` for lifecycle expectations
    - use `core/transport` when crossing process boundaries
    - keep module-specific business logic outside `core/events`
    - avoid inventing schema fields not documented here
    - update schemas and examples together
    - keep examples valid and copy/paste safe

    Cursor AI agents MUST NOT:
    - create hidden behavior
    - bypass validation
    - duplicate existing core functionality
    - place module-specific payloads inside core schemas
    - use generic `README.md` names inside subdirectories


---

# Final Rule

Events are not optional in HomePi.

Any service/module that publishes runtime changes MUST use documented HomePi event standards.

---

# System Status Vertical Slice

`apps/backend` publishes SSE frames at `GET /events` using `createEventEnvelope` from `@homepi/core-events`.

| Event | Purpose |
|-------|---------|
| `system_status_snapshot` | Initial authoritative status after connect |
| `system_status_delta` | Periodic status updates |
| `heartbeat` | Keep-alive |

Topic: `system.status`  
Source: `homepi-backend`

Example: `apps/backend/examples/sse-system-status-event.example.json`
