# HomePi Core Config Schemas

This folder contains the shared JSON schemas for `core/config`.

## Files

- `service-config.schema.json` — base service configuration contract
- `environment.schema.json` — shared environment variable contract
- `runtime-config.schema.json` — shared runtime paths, watchdog, and service behavior contract
- `override.schema.json` — runtime override payload contract

## Rule

Schemas in this folder define shared configuration infrastructure only.

Module-specific schemas belong with the owning module.

Example:

```text
modules/audio/shairport-sync/config/schema/shairport-config.schema.json
```

not:

```text
core/config/schema/shairport-config.schema.json
```
