# HomePi Core Metrics

## Purpose

Defines lightweight runtime metrics standards for counters, gauges, timings, transport metrics, and service performance.

---

# Responsibilities

- metric naming
- counters
- gauges
- timers
- labels
- metric snapshots
- performance telemetry

---

# Non-Responsibilities

This core module MUST NOT contain module-specific business logic or hardware-specific behavior.

---

# Directory Structure

```text
core/metrics/
├─ core-metrics-readme.md
├─ schema/
├─ ts/
├─ examples/
└─ rules/
```

---

# Schema Strategy

Schemas define metric samples, metric definitions, and metric snapshots.


    ---

    # Cursor AI Implementation Rules

    Cursor AI agents MUST:
    - read this README before implementing `core/metrics`
    - follow the documented folder structure
    - preserve naming conventions
    - use `core/logging` for structured logs
    - use `core/config` for configuration
    - use `core/runtime` for lifecycle expectations
    - use `core/transport` when crossing process boundaries
    - keep module-specific business logic outside `core/metrics`
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

`core/metrics` defines shared platform standards and reusable implementation primitives only.
