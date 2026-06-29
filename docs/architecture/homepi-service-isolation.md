# HomePi Service Isolation

## Primary rule

An unrelated service failure must not disrupt other HomePi modules.

## Client-facing module facades

Only two modules are user-facing:

| Module | Facade | Socket |
|--------|--------|--------|
| Home Audio | `homepi-audio` | `/run/homepi/audio/audio.sock` |
| Contact Sensors | `homepi-sensors` | `/run/homepi/sensors/sensors.sock` |

## Failure boundaries

| Failure | Expected isolation |
|---------|-------------------|
| PCM router offline | Zone control and AirPlay may continue; paging may degrade |
| Hi-Fi controller offline | PCM may run; sensors unaffected |
| HomeKit bridge offline | Contact detection continues in HomePi UI |
| Broker offline | Direct commands continue; live UI updates degrade |
| Database offline | Runtime hardware control continues where safe; settings writes fail cleanly |
| Health observer offline | Backend reports degraded health monitoring; modules continue |

## Communication rules

```text
Commands are direct.
Events are fanout.
Health is observational.
Logs are diagnostic.
```

The broker must never be required for direct commands.

## systemd dependencies

Prefer `Wants=` and `After=` over `Requires=` unless a service truly cannot start without a dependency.

Services should start, detect unavailable capabilities, report degraded state, and continue where safe.

## Registry

Service and module definitions live in [`core/service-registry/registry.json`](../../core/service-registry/registry.json).
