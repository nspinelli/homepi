# HomePi Core Runtime Schemas

This folder contains JSON schemas for structured runtime data.

## Files

- `service-manifest.schema.json` — service identity, systemd, runtime path, permission, watchdog, and health contract
- `health-state.schema.json` — health check state payload
- `runtime-status.schema.json` — current runtime/process status payload
- `watchdog-status.schema.json` — watchdog telemetry payload
- `install-result.schema.json` — install/uninstall/upgrade result payload
- `lifecycle-event.schema.json` — lifecycle state transition payload

## Rule

Runtime schemas define structured runtime DATA only.

Runtime behavior belongs in:
- `core-runtime-readme.md`
- systemd templates
- install templates
- lifecycle documentation

Do not try to encode all runtime behavior as JSON schema.
