# HomePi Core Transport NDJSON

## Purpose

NDJSON is the preferred framing format for local stream transports.

## Format

Each line is exactly one JSON object.

```text
{"version":1,"id":"msg-001","type":"event","payload":{}}
{"version":1,"id":"msg-002","type":"event","payload":{}}
```

## Rules

- One message per line.
- Messages MUST be UTF-8.
- Messages MUST NOT contain raw unescaped newlines.
- Each line MUST parse as valid JSON.
- Invalid frames MUST be logged.
- Partial frames MUST be buffered until newline.
