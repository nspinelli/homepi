# HomePi Core Storage

## Purpose

Defines persistence standards for SQLite, Prisma, migrations, repositories, backups, and data ownership.

---

# Responsibilities

- SQLite standards
- Prisma standards
- migration rules
- repository patterns
- backup/restore contracts
- persistence ownership
- schema versioning

---

# Non-Responsibilities

This core module MUST NOT contain module-specific business logic or hardware-specific behavior.

---

# Directory Structure

```text
core/storage/
├─ core-storage-readme.md
├─ schema/
├─ ts/
├─ examples/
└─ rules/
```

---

# Schema Strategy

Schemas define database manifests, migration manifests, backup manifests, and repository contracts.


    ---

    # Cursor AI Implementation Rules

    Cursor AI agents MUST:
    - read this README before implementing `core/storage`
    - follow the documented folder structure
    - preserve naming conventions
    - use `core/logging` for structured logs
    - use `core/config` for configuration
    - use `core/runtime` for lifecycle expectations
    - use `core/transport` when crossing process boundaries
    - keep module-specific business logic outside `core/storage`
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

`core/storage` defines shared platform standards and reusable implementation primitives only.
