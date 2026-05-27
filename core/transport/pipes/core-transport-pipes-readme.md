# HomePi Core Transport Pipes

## Purpose

Named pipes provide simple one-way streaming integration for tools that require FIFO-based communication.

## Use Cases

Allowed examples:
- metadata stream integration
- legacy command hooks
- external process output streams

## Rules

- Pipes MUST have documented directionality.
- Pipes MUST NOT be used as a general replacement for bidirectional sockets.
- Pipe readers MUST handle EOF safely.
- Pipe writers MUST handle missing readers safely.
- Pipe activity MUST be logged when meaningful.
