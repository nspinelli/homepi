# Paging Backend Module

This folder provides backend paging HTTP integrations for HomePi.

## Responsibilities

- Proxy paging config/status/chime/voice CRUD calls to `homepi-audio-paging` via `/run/homepi/audio-paging.sock`.
- Publish paging command envelopes to `core/events` for speak/chime and preview flows.
- Manage paging API key generation, hashing, and verification semantics.

## Files

- `paging-client.ts` - Unix socket RPC client for paging service methods.
- `paging-types.ts` - Shared paging request/response types.
- `paging-api-key.ts` - API key generation, scrypt hashing, and verification helpers.
- `paging-routes.ts` - `/api/audio/paging/*` REST routes.
- `paging-api-key-routes.ts` - `/api/audio/settings/paging-api-key*` and settings summary routes.
