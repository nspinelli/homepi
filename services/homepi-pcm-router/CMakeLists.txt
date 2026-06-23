cmake_minimum_required(VERSION 3.20)
project(homepi-pcm-router VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)
find_package(Threads REQUIRED)

set(HOMEPI_LOGGING_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/logging/cpp")
set(HOMEPI_STORAGE_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/storage/cpp")
set(HOMEPI_EVENTS_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/events/cpp")

add_subdirectory(${HOMEPI_STORAGE_CPP} ${CMAKE_BINARY_DIR}/homepi_core_storage)
add_subdirectory(${HOMEPI_EVENTS_CPP} ${CMAKE_BINARY_DIR}/homepi_core_events)

add_executable(homepi-pcm-router
  src/main.cpp
  src/service.cpp
  src/audio-profile-loader.cpp
  src/zone-enable-loader.cpp
  src/loopback-mapping.cpp
  src/frame-ring.cpp
  src/routing-state.cpp
  src/snapshot-builder.cpp
  src/audio-bridge.cpp
  src/unix-api-server.cpp
  src/event-subscriber.cpp
  src/json-utils.cpp
)

target_include_directories(homepi-pcm-router PRIVATE
  include
  ${HOMEPI_LOGGING_CPP}/include
  ${HOMEPI_STORAGE_CPP}/include
  ${HOMEPI_EVENTS_CPP}/include
  ${ALSA_INCLUDE_DIRS}
)

target_link_libraries(homepi-pcm-router PRIVATE
  homepi_core_storage
  homepi_core_events
  ${ALSA_LIBRARIES}
  Threads::Threads
)

target_link_directories(homepi-pcm-router PRIVATE ${ALSA_LIBRARY_DIRS})
target_compile_options(homepi-pcm-router PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS homepi-pcm-router RUNTIME DESTINATION bin)

enable_testing()
add_executable(test_routing_state tests/test-routing-state.cpp src/routing-state.cpp)
target_include_directories(test_routing_state PRIVATE include)
add_test(NAME test_routing_state COMMAND test_routing_state)
