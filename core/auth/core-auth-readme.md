# HomePi Core Auth

## Purpose

Defines shared authentication, authorization, permission, service identity, token, and local access standards.

---

# Responsibilities

- identity standards
- local service auth
- API auth
- WebSocket auth
- permission contracts
- token/session structures
- audit expectations

---

# Non-Responsibilities

This core module MUST NOT contain module-specific business logic or hardware-specific behavior.

---

# Directory Structure

```text
core/auth/
├─ core-auth-readme.md
├─ schema/
├─ ts/
├─ examples/
└─ rules/
```

---

# Schema Strategy

Schemas define principals, permissions, service identities, tokens, and sessions.


    ---

    # Cursor AI Implementation Rules

    Cursor AI agents MUST:
    - read this README before implementing `core/auth`
    - follow the documented folder structure
    - preserve naming conventions
    - use `core/logging` for structured logs
    - use `core/config` for configuration
    - use `core/runtime` for lifecycle expectations
    - use `core/transport` when crossing process boundaries
    - keep module-specific business logic outside `core/auth`
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

`core/auth` defines shared platform standards and reusable implementation primitives only.
