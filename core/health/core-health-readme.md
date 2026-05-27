# HomePi Core Health

## Purpose

Defines platform-level health, readiness, liveness, dependency checks, degraded states, and health aggregation.

---

# Responsibilities

- health contracts
- readiness/liveness rules
- dependency health
- aggregated health
- degraded state rules
- health event standards

---

# Non-Responsibilities

This core module MUST NOT contain module-specific business logic or hardware-specific behavior.

---

# Directory Structure

```text
core/health/
├─ core-health-readme.md
├─ schema/
├─ ts/
├─ examples/
└─ rules/
```

---

# Schema Strategy

Schemas define health reports, health checks, readiness, liveness, and aggregate health.


    ---

    # Cursor AI Implementation Rules

    Cursor AI agents MUST:
    - read this README before implementing `core/health`
    - follow the documented folder structure
    - preserve naming conventions
    - use `core/logging` for structured logs
    - use `core/config` for configuration
    - use `core/runtime` for lifecycle expectations
    - use `core/transport` when crossing process boundaries
    - keep module-specific business logic outside `core/health`
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

`core/health` defines shared platform standards and reusable implementation primitives only.

---

# System Status Vertical Slice

The first platform slice uses `createHealthReport` from `@homepi/core-health` for `GET /api/health` in `apps/backend`.

- Example response: `core/health/examples/health-report.example.json`
- Backend maps config/core readiness into health checks
- Aggregate statuses: `healthy`, `degraded`, `failed`

See `docs/system-status-vertical-slice-readme.md` for end-to-end flow and testing.
