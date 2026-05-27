# HomePi Core Discovery

## Purpose

Defines service, module, device, capability, and mDNS discovery standards for HomePi.

---

# Responsibilities

- service discovery
- module discovery
- capability manifests
- device discovery
- mDNS standards
- registration contracts

---

# Non-Responsibilities

This core module MUST NOT contain module-specific business logic or hardware-specific behavior.

---

# Directory Structure

```text
core/discovery/
├─ core-discovery-readme.md
├─ schema/
├─ ts/
├─ examples/
└─ rules/
```

---

# Schema Strategy

Schemas define discoverable services, modules, devices, and capabilities.


    ---

    # Cursor AI Implementation Rules

    Cursor AI agents MUST:
    - read this README before implementing `core/discovery`
    - follow the documented folder structure
    - preserve naming conventions
    - use `core/logging` for structured logs
    - use `core/config` for configuration
    - use `core/runtime` for lifecycle expectations
    - use `core/transport` when crossing process boundaries
    - keep module-specific business logic outside `core/discovery`
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

`core/discovery` defines shared platform standards and reusable implementation primitives only.
