# Include API

Public headers for the native `homepi-audio-paging` service modules.

- `types.hpp`: shared paging domain types.
- `service-config.hpp`: runtime config loading.
- `paging-repository.hpp`: SQLite persistence API.
- `unix-api-server.hpp`: local RPC socket contract.
- `events-subscriber.hpp`: broker command subscription.
- `piper-worker.hpp`, `pcm-player.hpp`, `chime-player.hpp`: audio pipeline primitives.
- `hifi-paging-controller.hpp`: HiFi page control and state tracking.
- `resource-manager.hpp`: warm/cold/active lifecycle.
- `orchestrator.hpp`: speak/chime sequencing.
- `service.hpp`: service runtime wrapper.
