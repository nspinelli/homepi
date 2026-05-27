# HomePi Core Runtime

## Purpose

The HomePi runtime system defines how HomePi services, modules, and supporting infrastructure execute on the operating system.

The runtime layer is responsible for:
- service lifecycle management
- systemd integration
- watchdog handling
- health monitoring
- runtime paths
- process isolation
- restart behavior
- runtime permissions
- install standards
- deployment consistency
- operational resilience

The runtime system exists to ensure every HomePi component behaves consistently, predictably, and safely in production.

---

# Philosophy

HomePi runtime architecture is designed around the following principles:

## Systemd Native

HomePi is designed specifically for Linux systems using systemd.

systemd is treated as:
- the process supervisor
- the lifecycle manager
- the watchdog coordinator
- the service orchestrator
- the logging integration layer

HomePi does NOT implement custom process supervision.

---

## Service Isolation

Each HomePi service runs independently.

Services should:
- fail independently
- restart independently
- log independently
- expose isolated runtime state

A failure in one service must not destabilize the entire system.

---

## Deterministic Runtime Behavior

Runtime behavior should always be:
- predictable
- observable
- reproducible
- AI-debuggable

Avoid:
- hidden startup dependencies
- uncontrolled background behavior
- implicit runtime assumptions

---

## Fail Fast

Services should fail immediately when:
- configuration is invalid
- dependencies are unavailable
- permissions are incorrect
- runtime requirements are unmet

Runtime instability is worse than startup failure.

---

## Minimal Runtime Complexity

HomePi avoids:
- custom supervisors
- custom service managers
- centralized daemon orchestration
- hidden runtime abstractions

The operating system should remain understandable and observable through standard Linux tooling.

---

# Responsibilities

The runtime system is responsible for:

- service lifecycle management
- startup sequencing
- restart behavior
- watchdog integration
- runtime path standards
- process isolation
- service dependencies
- environment loading
- runtime health monitoring
- install conventions
- runtime ownership/permissions
- systemd integration

---

# Non-Responsibilities

The runtime system is NOT responsible for:

- business logic
- hardware protocols
- module feature behavior
- API contracts
- transport parsing
- frontend rendering
- automation behavior

---

# Directory Structure

```text
core/runtime/
├─ core-runtime-readme.md
│
├─ systemd/
│  ├─ runtime-systemd-readme.md
│  ├─ templates/
│  ├─ services/
│  └─ targets/
│
├─ lifecycle/
│  ├─ runtime-lifecycle-readme.md
│  ├─ startup/
│  ├─ shutdown/
│  └─ recovery/
│
├─ watchdog/
│  ├─ runtime-watchdog-readme.md
│  ├─ cpp/
│  └─ ts/
│
├─ health/
│  ├─ runtime-health-readme.md
│  ├─ probes/
│  └─ checks/
│
├─ install/
│  ├─ runtime-install-readme.md
│  ├─ scripts/
│  ├─ templates/
│  └─ permissions/
│
├─ paths/
│  ├─ runtime-paths-readme.md
│  └─ standards/
│
├─ permissions/
│  ├─ runtime-permissions-readme.md
│  ├─ users/
│  └─ groups/
│
└─ examples/
   ├─ service-example.service
   ├─ watchdog-example.service
   └─ health-check-example.md
```

---

# Runtime Architecture

Each HomePi service operates as an isolated systemd-managed process.

Example:

```text
homepi-backend.service
homepi-hifi-serial.service
homepi-contact-sensors.service
homepi-airplay-owner.service
```

Each service:
- owns its runtime state
- owns its lifecycle
- owns its logs
- owns its restart behavior

---

# Service Naming Rules

All HomePi services MUST use the following naming convention:

```text
homepi-<service-name>.service
```

Examples:
- homepi-backend.service
- homepi-hifi-serial.service
- homepi-contact-sensors.service

This enables:
- deterministic observability
- easier AI debugging
- consistent journald filtering
- predictable deployment behavior

---

# Runtime Paths

Runtime paths MUST remain deterministic.

---

## Standard Runtime Paths

```text
/opt/homepi/
/opt/homepi/apps/
/opt/homepi/modules/
/opt/homepi/runtime/
/opt/homepi/runtime/generated/
/opt/homepi/runtime/cache/
/opt/homepi/runtime/state/
/opt/homepi/runtime/tmp/
```

---

## Socket Paths

```text
/run/homepi/
```

Examples:

```text
/run/homepi/hifi-serial.sock
/run/homepi/contact-sensors.sock
```

---

## Environment Paths

```text
/opt/homepi/services/<service>/env/.env
```

---

## Generated Runtime Config

```text
/opt/homepi/runtime/generated/
```

---

# Service Lifecycle

HomePi services follow the lifecycle:

```text
systemd starts service
↓
load configuration
↓
validate configuration
↓
initialize dependencies
↓
start transports
↓
emit service_started log
↓
begin runtime loop
↓
watchdog active
↓
runtime operation
↓
shutdown signal received
↓
graceful shutdown
↓
emit service_stopped log
↓
process exits
```

---

# Startup Rules

Services MUST:
- validate config before startup
- initialize dependencies deterministically
- emit startup logs
- fail immediately on invalid runtime state

Services MUST NOT:
- continue in partially initialized states
- silently ignore startup failures
- retry infinitely without limits

---

# Shutdown Rules

Services MUST support graceful shutdown.

Graceful shutdown includes:
- closing sockets
- flushing logs
- stopping threads
- releasing hardware resources
- saving runtime state if necessary

Shutdown events MUST be logged.

---

# Restart Behavior

systemd is responsible for restart behavior.

Recommended defaults:

```ini
Restart=on-failure
RestartSec=5
```

Services MUST NOT implement custom infinite restart loops internally.

---

# Watchdog Integration

HomePi uses:
- systemd watchdog
- internal runtime health checks

Watchdog integration exists to:
- detect deadlocks
- detect hung processes
- detect stalled event loops
- recover failed services

---

# Watchdog Rules

Services using watchdog support MUST:
- emit watchdog telemetry logs
- maintain heartbeat behavior
- fail cleanly on watchdog timeout

Examples:
- watchdog_ping
- watchdog_timeout
- watchdog_recovered

---

# Health Monitoring

Health systems should expose:
- dependency state
- transport state
- queue state
- runtime health
- module readiness

Health checks should be:
- lightweight
- deterministic
- structured
- observable

---

# Example Health States

```text
healthy
degraded
starting
stopping
failed
```

---

# Example Health Log

```json
{
  "ts": "2026-05-26T22:15:44.123Z",
  "service": "homepi-hifi-serial",
  "module": "core.runtime",
  "level": "INFO",
  "event": "health_state_changed",
  "correlationId": "runtime-health-check",
  "message": "Runtime health state changed",
  "data": {
    "previous": "starting",
    "new": "healthy"
  }
}
```

---

# Install System

Install scripts MUST be:
- deterministic
- idempotent
- non-interactive
- repeatable

Install scripts should:
- create required directories
- install systemd units
- set permissions
- reload systemd
- enable/start services
- validate runtime state

---

# Runtime Permissions

Services should run with the minimum required privileges.

Avoid:
- unnecessary root access
- broad filesystem permissions
- unrestricted hardware access

---

# Example Runtime User

```ini
User=homepi
Group=homepi
```

---

# Runtime Isolation

Services SHOULD use systemd hardening features when possible.

Examples:
- ProtectSystem
- ProtectHome
- PrivateTmp
- NoNewPrivileges

Hardening should not break required hardware access.

---

# Example Systemd Service

```ini
[Unit]
Description=HomePi HiFi Serial Service
After=network.target

[Service]
Type=simple
User=homepi
Group=homepi
ExecStart=/opt/homepi/services/hifi-serial/bin/homepi-hifi-serial
Restart=on-failure
RestartSec=5
WatchdogSec=30
EnvironmentFile=/opt/homepi/services/hifi-serial/env/.env

[Install]
WantedBy=multi-user.target
```

---

# Dependency Rules

Dependencies MUST remain explicit.

Avoid:
- hidden startup ordering
- circular dependencies
- implicit runtime assumptions

Use:
- Requires=
- Wants=
- After=

only when necessary.

---

# Logging Requirements

Runtime systems MUST emit structured logs for:
- service_started
- service_stopping
- service_stopped
- dependency_ready
- dependency_failed
- watchdog_timeout
- health_state_changed
- install_completed

---

# Example Startup Log

```json
{
  "ts": "2026-05-26T22:15:44.123Z",
  "service": "homepi-backend",
  "module": "core.runtime",
  "level": "INFO",
  "event": "service_started",
  "correlationId": "startup-runtime",
  "message": "Service startup completed",
  "data": {
    "pid": 1842,
    "watchdogEnabled": true
  }
}
```

---

# Runtime Recovery

Recovery systems SHOULD:
- remain deterministic
- avoid infinite retry storms
- emit recovery logs
- preserve observability

Recovery logic should remain minimal.

systemd should remain the primary recovery mechanism.

---

# AI Requirements

Runtime systems must remain AI-observable.

AI systems should be able to determine:
- why a service failed
- what dependency caused failure
- runtime state transitions
- restart behavior
- health state
- watchdog status

Runtime contracts should remain:
- deterministic
- explicit
- structured
- observable

---

# Cross-Language Consistency

Runtime behavior MUST remain consistent across:
- C++
- TypeScript
- install tooling
- systemd integration

This includes:
- lifecycle behavior
- logging behavior
- health reporting
- watchdog handling
- startup semantics

---

# Forbidden Practices

The following are forbidden:

## Custom Process Supervisors

systemd is the process supervisor.

---

## Hidden Runtime State

Runtime state must remain observable.

---

## Infinite Internal Retry Loops

Restart behavior belongs to systemd.

---

## Silent Failures

Runtime failures MUST emit structured ERROR logs.

---

## Hardcoded Runtime Paths

Runtime paths MUST remain centralized and deterministic.

---

# Documentation Rules

All runtime examples throughout HomePi documentation should be:
- operationally realistic
- deterministic
- copy/paste safe
- AI-readable
- implementation accurate

Documentation is considered part of the runtime contract.

---

# Future Expansion

Future runtime enhancements may include:
- container support
- distributed runtime orchestration
- cluster-aware deployments
- runtime capability discovery
- service mesh integration

These systems MUST preserve:
- explicit runtime behavior
- deterministic lifecycle management
- structured observability
- service isolation

---

# Final Rule

The HomePi runtime system exists to provide deterministic, observable, resilient service execution across the entire HomePi platform.

Every HomePi service, module, transport layer, runtime dependency, install process, and hardware integration MUST conform to the shared HomePi runtime architecture.