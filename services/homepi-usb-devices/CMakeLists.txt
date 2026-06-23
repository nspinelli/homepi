cmake_minimum_required(VERSION 3.16)
project(homepi-usb-devices VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)
pkg_check_modules(UDEV REQUIRED libudev)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

set(HOMEPI_LOGGING_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/logging/cpp")
set(HOMEPI_STORAGE_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/storage/cpp")
set(HOMEPI_EVENTS_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/events/cpp")

add_subdirectory(${HOMEPI_STORAGE_CPP} ${CMAKE_BINARY_DIR}/homepi_core_storage)
add_subdirectory(${HOMEPI_EVENTS_CPP} ${CMAKE_BINARY_DIR}/homepi_core_events)

add_executable(homepi-usb-devices
  src/main.cpp
  src/config-loader.cpp
  src/device-id.cpp
  src/device-scanner.cpp
  src/udev-monitor.cpp
  src/assignment-repository.cpp
  src/artifact-writer.cpp
  src/service-health-emitter.cpp
  src/unix-api-server.cpp
  src/post-assignment-hook.cpp
  src/alsa-capability-probe.cpp
  src/platform-loopback-policy.cpp
  src/audio-profile-validator.cpp
  src/audio-profile-writer.cpp
  src/audio-profile-service.cpp
  src/usb-event-emitter.cpp
)

target_include_directories(homepi-usb-devices PRIVATE
  include
  ${HOMEPI_LOGGING_CPP}/include
  ${HOMEPI_STORAGE_CPP}/include
  ${HOMEPI_EVENTS_CPP}/include
  ${UDEV_INCLUDE_DIRS}
  ${ALSA_INCLUDE_DIRS}
)

target_link_libraries(homepi-usb-devices PRIVATE
  homepi_core_storage
  homepi_core_events
  ${UDEV_LIBRARIES}
  ${ALSA_LIBRARIES}
  SQLite::SQLite3
  Threads::Threads
)

target_link_directories(homepi-usb-devices PRIVATE ${ALSA_LIBRARY_DIRS})
target_compile_options(homepi-usb-devices PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS homepi-usb-devices RUNTIME DESTINATION bin)
