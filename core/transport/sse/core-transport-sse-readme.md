# HomePi Core Transport SSE

## Purpose

Server-Sent Events provide one-way backend-to-browser streaming.

## Use Cases

- dashboard updates
- health feeds
- log/event streams
- read-only live state

## Rules

- SSE is one-way only.
- Clients MUST reconnect safely.
- Events SHOULD use structured JSON payloads.
- Event names MUST be stable.
- Backend must handle disconnected clients safely.
