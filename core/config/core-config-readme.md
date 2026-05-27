# HomePi Core Configuration

## Purpose

The HomePi configuration system provides the shared configuration infrastructure used across all HomePi services, modules, runtimes, and deployment environments.

Configuration is considered a core architectural primitive.

The configuration system exists to provide:
- centralized configuration standards
- schema validation
- deterministic startup behavior
- typed configuration access
- environment consistency
- deployment consistency
- module isolation
- runtime safety
- AI-readable configuration contracts

All HomePi services and modules MUST use the shared configuration framework.

---

# Philosophy

HomePi configuration is designed around the following principles:

## Schema First

All configuration MUST be schema validated before runtime usage.

Schemas are considered:
- runtime contracts
- operational documentation
- AI context
- deployment guarantees
- validation definitions

---

## Explicit Over Implicit

Configuration should always be:
- predictable
- visible
- documented
- deterministic

Avoid:
- hidden defaults
- implicit runtime behavior
- magic environment assumptions
- silent coercion

---

## Fail Fast

Invalid configuration MUST prevent service startup.

HomePi services should never operate in partially invalid states.

Startup validation failures are preferred over runtime instability.

---

## Modular Ownership

Core configuration provides:
- standards
- validation
- loaders
- normalization
- environment handling

Modules own:
- actual configuration
- module schemas
- runtime config files
- templates
- deployment artifacts

This separation is mandatory.

---

# Architecture

The HomePi configuration architecture is divided into two layers:

## Core Configuration Infrastructure

Located in:

```text
core/config/
```

Responsible for:
- config loading
- schema validation
- normalization
- environment handling
- override behavior
- typed access
- shared runtime standards

---

## Module Configuration

Located inside the owning module/service.

Examples:

```text
modules/audio/shairport-sync/config/
modules/audio/snapcast/config/
modules/audio/hifi-serial/config/
modules/sensors/contact-sensors/config/
```

Responsible for:
- module-specific settings
- runtime config files
- templates
- generated config
- deployment configuration
- module schemas

---

# Important Architectural Rule

Core configuration defines HOW configuration works.

Modules define WHAT gets configured.

---

# Responsibilities

The core configuration system is responsible for:

- configuration loading
- schema validation
- environment variable parsing
- configuration normalization
- typed configuration access
- startup validation
- configuration merging
- override handling
- cross-language consistency
- shared configuration standards

---

# Non-Responsibilities

The core configuration system is NOT responsible for:

- module business logic
- runtime state persistence
- hardware behavior
- protocol parsing
- module-specific runtime files
- Shairport-Sync operational config
- Snapcast operational config
- NGINX runtime config

Those belong inside the owning module/service.

---

# Directory Structure

```text
core/config/
├─ README.md
│
├─ schema/
│  ├─ service-config.schema.json
│  ├─ environment.schema.json
│  ├─ runtime-config.schema.json
│  └─ override.schema.json
│
├─ ts/
│  ├─ package.json
│  └─ src/
│     ├─ load-config.ts
│     ├─ validate-config.ts
│     ├─ normalize-config.ts
│     ├─ merge-config.ts
│     ├─ env-loader.ts
│     ├─ config-cache.ts
│     └─ config-types.ts
│
├─ cpp/
│  ├─ include/
│  │  └─ core/
│  │     └─ config/
│  │        ├─ config.hpp
│  │        ├─ env_loader.hpp
│  │        └─ validator.hpp
│  └─ src/
│
├─ runtime/
│  ├─ README.md
│  ├─ env/
│  ├─ defaults/
│  └─ overrides/
│
├─ examples/
│  ├─ service-env.example
│  ├─ runtime-config.example.json
│  └─ override.example.json
│
└─ rules/
   └─ config-rules.md
```

---

# Related Module Configuration Locations

Examples of module-owned configuration:

```text
modules/audio/shairport-sync/config/
├─ README.md
├─ schema/
├─ templates/
├─ generated/
└─ runtime/

modules/audio/snapcast/config/
modules/audio/hifi-serial/config/
modules/sensors/contact-sensors/config/
```

These directories belong to the owning module because the configuration is operationally tied to that module.

---

# Documentation Structure

Each configuration area MUST contain its own README.

Examples:

```text
core/config/README.md
modules/audio/shairport-sync/config/README.md
modules/audio/hifi-serial/config/README.md
```

These README files act as:
- source-of-truth documentation
- runtime contracts
- operational specifications
- deployment documentation
- AI guidance

---

# Configuration Layers

Configuration precedence order:

```text
Defaults
↓
Runtime Config
↓
Service Config
↓
Module Config
↓
Environment Variables
↓
Runtime Overrides
```

Higher layers override lower layers.

---

# Configuration Types

## Runtime Configuration

Defines:
- install paths
- socket paths
- runtime directories
- watchdog settings
- journald integration
- service runtime behavior

---

## Service Configuration

Defines:
- backend behavior
- frontend behavior
- API settings
- gateway behavior
- runtime networking

---

## Module Configuration

Defines:
- hardware integration settings
- transport settings
- protocol behavior
- runtime module behavior
- feature flags

Examples:
- Shairport-Sync
- Snapcast
- HiFi2
- Sensors
- TTS

---

## Environment Configuration

Defines:
- environment variables
- deployment overrides
- runtime environment values
- runtime-specific settings

---

# Schema Validation

All configuration MUST be validated before service startup.

Validation failures MUST:
- prevent startup
- emit structured ERROR logs
- explain validation failures clearly

Services MUST NOT:
- continue with invalid config
- silently ignore invalid values
- auto-correct unknown settings

---

# Example Validation Failure

```json
{
  "ts": "2026-05-26T22:15:44.123Z",
  "service": "homepi-backend",
  "module": "core.config",
  "level": "ERROR",
  "event": "config_invalid",
  "correlationId": "startup-config-validation",
  "message": "Configuration validation failed",
  "data": {
    "field": "PORT",
    "received": "abc",
    "expected": "integer"
  }
}
```

---

# Environment Variables

Environment variables MUST:
- be explicitly documented
- be schema validated
- use uppercase snake_case
- avoid hidden behavior

---

## Good Examples

```text
PORT
LOG_LEVEL
WATCHDOG_TIMEOUT_MS
SOCKET_PATH
```

---

## Bad Examples

```text
port
stuff
config2
```

---

# Environment File Rules

Environment files should exist at predictable locations.

Example:

```text
/opt/homepi/services/hifi-serial/env/.env
```

Environment loading should occur through:
- core/config loaders
- schema validation
- normalized typed access

NOT through:
- scattered getenv calls
- random process.env access

---

# Configuration Access Rules

Services and modules MUST access configuration through validated configuration objects.

Forbidden:

```ts
process.env.PORT
```

```cpp
std::getenv("PORT")
```

Allowed:

```ts
config.port
```

```cpp
config.port
```

after validation and normalization.

---

# Shared Configuration Infrastructure

The following belongs inside core/config:

- shared loaders
- shared validation
- environment parsing
- config normalization
- override merging
- typed config access
- shared runtime standards

---

# Module Configuration Ownership

The following belongs inside modules/services:

- shairport-sync.conf
- snapserver.conf
- nginx.conf
- sensor maps
- module runtime templates
- generated runtime configs

Example:

```text
modules/audio/shairport-sync/config/runtime/shairport-sync.conf
```

NOT:

```text
core/config/shairport-sync.conf
```

---

# Configuration Normalization

Configuration normalization converts validated raw configuration into deterministic runtime-safe values.

Normalization may include:
- type coercion
- path normalization
- unit conversion
- default expansion
- enum normalization
- derived runtime values

Examples:
- converting relative paths to absolute paths
- converting strings to integers
- normalizing socket paths
- expanding runtime directories

Normalization occurs AFTER validation and BEFORE runtime access.

---

# Configuration Merge Rules

Configuration merging MUST be deterministic.

Merge order:

Defaults
↓
Runtime Config
↓
Service Config
↓
Module Config
↓
Environment Variables
↓
Runtime Overrides

Higher layers override lower layers.

Merging MUST:
- preserve schema validity
- remain deterministic
- emit structured logs
- avoid hidden mutations

---

# Example Merge Result

```json
{
  "ts": "2026-05-26T22:15:44.123Z",
  "service": "homepi-backend",
  "module": "core.config",
  "level": "INFO",
  "event": "runtime_override_applied",
  "correlationId": "startup-config-merge",
  "message": "Runtime override applied successfully",
  "data": {
    "field": "LOG_LEVEL",
    "previous": "INFO",
    "new": "DEBUG"
  }
}
```

---

# Typed Configuration Access

Configuration MUST be exposed through typed configuration objects.

Examples:

TypeScript:

```ts
config.serial.baudRate
config.logging.level
```

C++:

```cpp
config.serial.baud_rate
config.logging.level
```

Services and modules MUST NOT:
- parse raw JSON repeatedly
- read raw environment variables throughout the codebase
- bypass validation layers

---

# Runtime Configuration Paths

Runtime configuration paths should remain predictable and deterministic.

Examples:

```text
/opt/homepi/
/opt/homepi/services/
/opt/homepi/modules/
/opt/homepi/runtime/
/opt/homepi/runtime/config/
/opt/homepi/runtime/generated/
```

Environment files:

```text
/opt/homepi/services/<service>/env/.env
```

Generated runtime configuration:

```text
/opt/homepi/runtime/generated/
```

Socket/runtime paths:

```text
/run/homepi/
```

---

# Generated Configuration

Generated configuration files SHOULD NOT be edited manually.

Generated files should exist in predictable runtime locations.

Examples:
- generated Shairport configs
- generated Snapcast configs
- generated runtime manifests
- generated module runtime files

Recommended structure:

```text
modules/audio/shairport-sync/config/generated/
```

Generated configuration should:
- be reproducible
- be deterministic
- emit generation logs
- include source metadata when possible

---

# Configuration Templates

Templates define reusable configuration blueprints.

Templates SHOULD:
- remain source controlled
- avoid runtime secrets
- remain deterministic
- support environment substitution

Examples:
- shairport-sync.conf.template
- nginx-site.template
- snapserver.template.conf

Templates belong to the owning module/service.

---

# Configuration Caching

Configuration loaders MAY cache validated configuration objects.

Caching should:
- reduce repeated parsing
- reduce repeated validation
- improve runtime performance

Cached configuration MUST:
- remain immutable
- remain schema valid
- preserve deterministic behavior

---

# Dynamic Reloading

Dynamic configuration reloads are OPTIONAL.

Services supporting runtime reloads MUST:
- validate updated config before applying
- reject invalid updates
- emit structured logs
- preserve runtime safety

Hot reload support must never bypass validation.

---

# Configuration Versioning

Configuration schemas SHOULD eventually support explicit versioning.

Example:

```json
{
  "version": 1,
  "config": {}
}
```

Versioning allows:
- migrations
- backwards compatibility
- schema evolution
- runtime upgrades

---

# Example Core Loader Responsibility

The following belongs inside `core/config`:

- loading `.env`
- validating schema
- normalizing values
- merging overrides
- producing typed runtime objects

The following does NOT belong inside `core/config`:

- Shairport runtime generation
- Snapcast runtime generation
- NGINX runtime files
- hardware-specific configuration logic

---

# Secrets Rules

Secrets MUST:
- remain external to source control
- never be hardcoded
- never be logged
- never appear in DEBUG payloads

Examples:
- API keys
- tokens
- certificates
- passwords

---

# Runtime Overrides

Runtime overrides should support:
- temporary debugging
- deployment overrides
- hardware replacement
- runtime testing

without modifying source-controlled defaults.

---

# Cross-Language Consistency

Configuration behavior MUST remain consistent across:
- TypeScript
- C++
- runtime scripts
- install tooling

This includes:
- validation behavior
- naming
- normalization
- merge behavior
- override behavior

---

# Example Module Configuration

```json
{
  "enabled": true,
  "device": "/dev/ttyUSB0",
  "baudRate": 9600,
  "timeoutMs": 50,
  "virtualPort": "/dev/vHifi"
}
```

---

# Logging Requirements

Configuration systems MUST emit structured logs for:
- config_loaded
- config_invalid
- env_loaded
- defaults_applied
- runtime_override_applied

---

# Example Successful Load

```json
{
  "ts": "2026-05-26T22:15:44.123Z",
  "service": "homepi-hifi-serial",
  "module": "core.config",
  "level": "INFO",
  "event": "config_loaded",
  "correlationId": "startup-config-load",
  "message": "Configuration loaded successfully",
  "data": {
    "environment": "production",
    "configPath": "/opt/homepi/services/hifi-serial/env/.env"
  }
}
```

---

# AI Requirements

Configuration systems must remain AI-readable.

This includes:
- deterministic schemas
- explicit naming
- predictable structure
- runtime traceability
- structured validation errors

Configuration contracts should allow AI systems to:
- understand runtime behavior
- validate deployments
- generate configs safely
- debug startup failures
- reason about overrides

---

# Example Configuration Lifecycle

```text
Service Startup
↓
Load Environment
↓
Load Runtime Config
↓
Load Module Config
↓
Merge Configurations
↓
Validate Schema
↓
Normalize Values
↓
Create Typed Config Object
↓
Emit config_loaded Log
↓
Start Runtime
```

---

# JSON Requirements

All JSON configuration files MUST:
- be UTF-8 encoded
- validate against schema
- avoid duplicate keys
- remain deterministic
- avoid comments

---

# Forbidden Practices

The following are forbidden:

## Hidden Defaults

Configuration defaults MUST be documented.

---

## Silent Failures

Invalid config MUST fail loudly.

---

## Unvalidated Configuration

All config MUST be schema validated.

---

## Scattered Environment Access

Environment variables MUST NOT be accessed randomly throughout the codebase.

---

## Logging Secrets

Secrets MUST NEVER appear in logs.

---

# Documentation Rules

All configuration examples throughout HomePi documentation should be fully valid unless explicitly marked otherwise.

Documentation examples are considered architectural contracts.

Examples should be:
- schema compliant
- copy/paste safe
- operationally realistic
- AI-readable
- implementation accurate

---

# Future Expansion

Future configuration systems may include:
- YAML support
- encrypted secrets
- remote config providers
- hot reload
- distributed configuration
- config versioning

These systems must preserve:
- schema validation
- deterministic behavior
- operational traceability
- explicit contracts

---

# Final Architectural Rule

Core configuration exists to provide a shared configuration framework.

It does NOT exist to centrally store all runtime configuration files.

Module-specific configuration must remain owned by the module/service responsible for that behavior.

This separation preserves:
- modularity
- maintainability
- deployment isolation
- runtime ownership
- long-term scalability

---

# Final Rule

Configuration is not optional in HomePi.

Every service, module, runtime component, install script, transport layer, and hardware integration MUST use the shared HomePi configuration standards and validation system.
