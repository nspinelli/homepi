cmake_minimum_required(VERSION 3.20)
project(homepi-audio-paging VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(ALSA alsa)
endif()

set(HOMEPI_LOGGING_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/logging/cpp")
set(HOMEPI_STORAGE_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/storage/cpp")
set(HOMEPI_EVENTS_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/events/cpp")

add_subdirectory(${HOMEPI_STORAGE_CPP} ${CMAKE_BINARY_DIR}/homepi_core_storage)
add_subdirectory(${HOMEPI_EVENTS_CPP} ${CMAKE_BINARY_DIR}/homepi_core_events)

add_executable(homepi-audio-paging
  src/main.cpp
  src/service-config.cpp
  src/json-utils.cpp
  src/paging-repository.cpp
  src/unix-api-server.cpp
  src/events-subscriber.cpp
  src/usb-assignments-client.cpp
  src/piper-worker.cpp
  src/pcm-player.cpp
  src/dac-volume.cpp
  src/chime-player.cpp
  src/hifi-paging-controller.cpp
  src/resource-manager.cpp
  src/orchestrator.cpp
  src/service.cpp
)

target_include_directories(homepi-audio-paging PRIVATE
  include
  ${HOMEPI_LOGGING_CPP}/include
  ${HOMEPI_STORAGE_CPP}/include
  ${HOMEPI_EVENTS_CPP}/include
)

if(ALSA_FOUND)
  target_include_directories(homepi-audio-paging PRIVATE ${ALSA_INCLUDE_DIRS})
  target_link_directories(homepi-audio-paging PRIVATE ${ALSA_LIBRARY_DIRS})
  target_link_libraries(homepi-audio-paging PRIVATE ${ALSA_LIBRARIES})
  target_compile_definitions(homepi-audio-paging PRIVATE HOMEPI_AUDIO_PAGING_HAS_ALSA=1)
endif()

target_link_libraries(homepi-audio-paging PRIVATE
  homepi_core_storage
  homepi_core_events
  SQLite::SQLite3
  Threads::Threads
)

target_compile_options(homepi-audio-paging PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS homepi-audio-paging RUNTIME DESTINATION bin)
