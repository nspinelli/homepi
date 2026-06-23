cmake_minimum_required(VERSION 3.20)
project(homepi-audio-orchestrator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

set(HOMEPI_EVENTS_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/events/cpp")
set(HOMEPI_TRANSPORT_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/transport/cpp")

add_subdirectory(${HOMEPI_EVENTS_CPP} ${CMAKE_BINARY_DIR}/homepi_core_events)

add_executable(homepi-audio-orchestrator
  src/main.cpp
  src/orchestrator.cpp
  src/service-socket-client.cpp
  src/service-config.cpp
  src/json-utils.cpp
  src/airplay-source-loader.cpp
)

target_include_directories(homepi-audio-orchestrator PRIVATE
  include
  ${HOMEPI_EVENTS_CPP}/include
  ${HOMEPI_TRANSPORT_CPP}/include
)

target_link_libraries(homepi-audio-orchestrator PRIVATE
  homepi_core_events
  homepi_core_transport
  SQLite::SQLite3
  Threads::Threads
)

target_compile_options(homepi-audio-orchestrator PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS homepi-audio-orchestrator RUNTIME DESTINATION bin)
