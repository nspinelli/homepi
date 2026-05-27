# HomePi Core Api

## Purpose

Defines shared API standards for HTTP, REST, response envelopes, errors, pagination, versioning, and backend gateway behavior.

---

# Responsibilities

- API envelopes
- error responses
- versioning rules
- pagination standards
- request correlation
- gateway standards
- OpenAPI compatibility

---

# Non-Responsibilities

This core module MUST NOT contain module-specific business logic or hardware-specific behavior.

---

# Directory Structure

```text
core/api/
├─ core-api-readme.md
├─ schema/
├─ ts/
├─ examples/
└─ rules/
```

---

# Schema Strategy

Schemas define API response envelopes, error payloads, pagination payloads, and request metadata.


    ---

    # Cursor AI Implementation Rules

    Cursor AI agents MUST:
    - read this README before implementing `core/api`
    - follow the documented folder structure
    - preserve naming conventions
    - use `core/logging` for structured logs
    - use `core/config` for configuration
    - use `core/runtime` for lifecycle expectations
    - use `core/transport` when crossing process boundaries
    - keep module-specific business logic outside `core/api`
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

`core/api` defines shared platform standards and reusable implementation primitives only.

---

# System Status Vertical Slice

`apps/backend` returns all REST payloads through `createSuccessResponse` / `createErrorResponse` with a required `correlationId` from `getRequestCorrelationId` + `resolveCorrelationId`.

Examples:

- `core/api/examples/api-response.example.json`
- `apps/backend/examples/core-status-response.example.json`

Every status route (`/api/health`, `/api/runtime/status`, `/api/core/status`) uses the API response envelope.
