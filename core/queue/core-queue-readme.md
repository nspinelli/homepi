# HomePi Core Queue

## Purpose

Defines shared async queue, command queue, retry queue, bounded queue, and backpressure standards.

---

# Responsibilities

- queue contracts
- retry policies
- backpressure rules
- command sequencing
- timeout behavior
- queue health
- queue metrics

---

# Non-Responsibilities

This core module MUST NOT contain module-specific business logic or hardware-specific behavior.

---

# Directory Structure

```text
core/queue/
├─ core-queue-readme.md
├─ schema/
├─ ts/
├─ examples/
└─ rules/
```

---

# Schema Strategy

Schemas define queue items, queue status, retry policy, and worker status.


    ---

    # Cursor AI Implementation Rules

    Cursor AI agents MUST:
    - read this README before implementing `core/queue`
    - follow the documented folder structure
    - preserve naming conventions
    - use `core/logging` for structured logs
    - use `core/config` for configuration
    - use `core/runtime` for lifecycle expectations
    - use `core/transport` when crossing process boundaries
    - keep module-specific business logic outside `core/queue`
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

`core/queue` defines shared platform standards and reusable implementation primitives only.
