cmake_minimum_required(VERSION 3.16)
project(homepi-shairport-sync VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

set(HOMEPI_LOGGING_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/logging/cpp")
set(HOMEPI_STORAGE_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/storage/cpp")
set(HOMEPI_EVENTS_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/events/cpp")
set(HOMEPI_TRANSPORT_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/transport/cpp")

add_subdirectory(${HOMEPI_STORAGE_CPP} ${CMAKE_BINARY_DIR}/homepi_core_storage)
add_subdirectory(${HOMEPI_EVENTS_CPP} ${CMAKE_BINARY_DIR}/homepi_core_events)

add_executable(homepi-shairport-supervisor
  src/main.cpp
  src/config-loader.cpp
  src/db-repository.cpp
  src/readiness-gate.cpp
  src/config-generator.cpp
  src/systemd-controller.cpp
  src/hifi-event-client.cpp
  src/usb-event-client.cpp
  src/supervisor.cpp
)

target_include_directories(homepi-shairport-supervisor PRIVATE
  include
  ${HOMEPI_LOGGING_CPP}/include
  ${HOMEPI_STORAGE_CPP}/include
)

target_link_libraries(homepi-shairport-supervisor PRIVATE
  homepi_core_storage
  SQLite::SQLite3
  Threads::Threads
)

target_compile_options(homepi-shairport-supervisor PRIVATE -Wall -Wextra -Wpedantic)

add_executable(homepi-shairport-hook
  src/shairport-hook-main.cpp
)

target_include_directories(homepi-shairport-hook PRIVATE
  ${HOMEPI_EVENTS_CPP}/include
  ${HOMEPI_TRANSPORT_CPP}/include
)

target_link_libraries(homepi-shairport-hook PRIVATE
  homepi_core_events
)

target_compile_options(homepi-shairport-hook PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS homepi-shairport-supervisor homepi-shairport-hook RUNTIME DESTINATION bin)
