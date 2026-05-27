# HomePi Core Transport C++

## Purpose

The C++ transport implementation provides shared helpers for native HomePi daemons.

It should provide:
- Unix socket server/client helpers
- NDJSON encoder/decoder
- transport envelope structures
- transport error helpers
- connection lifecycle helpers
- backpressure handling primitives

## Expected Source Layout

```text
core/transport/cpp/
├─ core-transport-cpp-readme.md
├─ CMakeLists.txt
└─ include/core/transport/
   ├─ envelope.hpp
   ├─ errors.hpp
   ├─ ndjson.hpp
   ├─ unix_socket_server.hpp
   ├─ unix_socket_client.hpp
   └─ backpressure.hpp
```

## Rules

C++ services MUST emit valid single-line JSON messages when using stream transports.
