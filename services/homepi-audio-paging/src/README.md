# Source Modules

Implementation files for the `homepi-audio-paging` native daemon.

Core flow:

1. `service.cpp` wires dependencies and starts sockets/subscriptions.
2. `events-subscriber.cpp` receives broker commands.
3. `orchestrator.cpp` executes TTS + PAGE ON/OFF + playback sequence.
4. `paging-repository.cpp` persists config/voice/chime/job state.
5. `unix-api-server.cpp` serves local RPC methods for backend integration.
