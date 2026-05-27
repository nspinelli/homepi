# HomePi Core Transport Rules

## Required Rules

- Use documented transport contracts.
- Use structured transport envelopes where applicable.
- Use NDJSON for local stream framing unless explicitly documented otherwise.
- Log all meaningful lifecycle transitions.
- Validate inbound messages before handling.
- Keep module-specific payload meaning out of core transport.
- Use bounded reconnect behavior.
- Define backpressure behavior.

## Forbidden

- Silent disconnects.
- Unbounded queues.
- Infinite reconnect storms.
- Free-form unframed streams.
- Module protocol parsing inside core transport.
