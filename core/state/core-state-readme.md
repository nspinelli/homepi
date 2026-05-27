# HomePi Core State

## Purpose

The HomePi state system defines how authoritative runtime state is represented, synchronized, snapshotted, cached, and exposed.

State is separate from events.

Events describe what changed.

State describes what is currently true.

---

# Philosophy

## Authoritative Ownership

Every state object MUST have one authoritative owner.

## Snapshot First

Stateful services SHOULD provide a snapshot on client connection before emitting delta events.

## Immutable Boundaries

Consumers should treat state snapshots as immutable.

## Persistence Boundary

Core state does not decide what is stored permanently.

`core/storage` handles persistence standards.

---

# Responsibilities

`core/state` is responsible for:
- state envelope contracts
- snapshot contracts
- state ownership rules
- delta rules
- state sync semantics
- cache rules
- stale state rules

---

# Directory Structure

```text
core/state/
├─ core-state-readme.md
├─ schema/
│  ├─ core-state-schema-readme.md
│  ├─ state-envelope.schema.json
│  ├─ state-snapshot.schema.json
│  ├─ state-delta.schema.json
│  └─ state-owner.schema.json
├─ ts/
│  └─ core-state-ts-readme.md
├─ cpp/
│  └─ core-state-cpp-readme.md
├─ snapshots/
│  └─ core-state-snapshots-readme.md
├─ stores/
│  └─ core-state-stores-readme.md
├─ sync/
│  └─ core-state-sync-readme.md
├─ examples/
└─ rules/
   └─ core-state-rules-readme.md
```

---

# Final Rule

State must always be owned, observable, and synchronized through documented contracts.

---

# System Status Vertical Slice

`apps/backend` keeps authoritative platform status in memory via `createSnapshot` (`owner: homepi-backend`, `topic: system.status`).

Example snapshot: `core/state/examples/system-status-snapshot.example.json`

The snapshot backs:

- `GET /api/core/status` (`data.system`)
- SSE `system_status_*` payloads
- WebSocket initial `type: "snapshot"` envelopes


    ---

    # Cursor AI Implementation Rules

    Cursor AI agents MUST:
    - read this README before implementing `core/state`
    - follow the documented folder structure
    - preserve naming conventions
    - use `core/logging` for structured logs
    - use `core/config` for configuration
    - use `core/runtime` for lifecycle expectations
    - use `core/transport` when crossing process boundaries
    - keep module-specific business logic outside `core/state`
    - avoid inventing schema fields not documented here
    - update schemas and examples together
    - keep examples valid and copy/paste safe

    Cursor AI agents MUST NOT:
    - create hidden behavior
    - bypass validation
    - duplicate existing core functionality
    - place module-specific payloads inside core schemas
    - use generic `README.md` names inside subdirectories
