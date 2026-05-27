# HomePi Core Transport Unix Sockets

## Purpose

Unix domain sockets are the preferred local IPC transport for HomePi service-to-service communication.

## Standard Path

```text
/run/homepi/<service>.sock
```

Example:

```text
/run/homepi/hifi-serial.sock
```

## Responsibilities

Unix socket helpers should support:
- server creation
- client connection
- reconnect behavior
- lifecycle logging
- NDJSON framing
- graceful shutdown
- permission-safe socket creation

## Rules

- Sockets MUST live under `/run/homepi/`.
- Socket creation MUST be logged.
- Client connect/disconnect MUST be logged.
- Slow clients MUST be handled explicitly.
- Module-specific payload meaning MUST remain outside core transport.
