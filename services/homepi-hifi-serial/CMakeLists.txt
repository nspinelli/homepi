cmake_minimum_required(VERSION 3.16)
project(homepi-hifi-serial VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

set(HOMEPI_LOGGING_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/logging/cpp")

add_executable(homepi-hifi-serial
  src/main.cpp
  src/config-loader.cpp
  src/serial-path-resolver.cpp
  src/serial-port.cpp
  src/command-queue.cpp
  src/protocol-encoder.cpp
  src/protocol-parser.cpp
  src/state-repository.cpp
  src/sync-engine.cpp
  src/event-publisher.cpp
  src/service-health-emitter.cpp
  src/unix-api-server.cpp
)

target_include_directories(homepi-hifi-serial PRIVATE
  include
  ${HOMEPI_LOGGING_CPP}/include
)

target_link_libraries(homepi-hifi-serial PRIVATE
  SQLite::SQLite3
  Threads::Threads
)

target_compile_options(homepi-hifi-serial PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS homepi-hifi-serial RUNTIME DESTINATION bin)

enable_testing()
add_executable(homepi-hifi-serial-protocol-test
  tests/protocol-test.cpp
  src/protocol-encoder.cpp
  src/protocol-parser.cpp
)
target_include_directories(homepi-hifi-serial-protocol-test PRIVATE include)
target_compile_options(homepi-hifi-serial-protocol-test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME protocol COMMAND homepi-hifi-serial-protocol-test)
