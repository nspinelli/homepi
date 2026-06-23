cmake_minimum_required(VERSION 3.20)
project(homepi-metadata VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

set(HOMEPI_LOGGING_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/logging/cpp")
set(HOMEPI_STORAGE_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/storage/cpp")
set(HOMEPI_EVENTS_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/events/cpp")
set(HOMEPI_TRANSPORT_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/transport/cpp")

add_subdirectory(${HOMEPI_STORAGE_CPP} ${CMAKE_BINARY_DIR}/homepi_core_storage)
add_subdirectory(${HOMEPI_EVENTS_CPP} ${CMAKE_BINARY_DIR}/homepi_core_events)

add_executable(homepi-metadata
  src/main.cpp
  src/service-config.cpp
  src/service.cpp
  src/events-owner-subscriber.cpp
  src/metadata-coalescer.cpp
  src/realtime-progress-server.cpp
  src/pipe-manager.cpp
  src/metadata-parser.cpp
  src/progress-parser.cpp
  src/now-playing-state.cpp
  src/metadata-state-repository.cpp
  src/snapshot-builder.cpp
  src/unix-api-server.cpp
  src/json-utils.cpp
)

target_include_directories(homepi-metadata PRIVATE
  include
  ${HOMEPI_LOGGING_CPP}/include
  ${HOMEPI_STORAGE_CPP}/include
  ${HOMEPI_EVENTS_CPP}/include
  ${HOMEPI_TRANSPORT_CPP}/include
)

target_link_libraries(homepi-metadata PRIVATE
  homepi_core_storage
  homepi_core_events
  Threads::Threads
)

target_compile_options(homepi-metadata PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS homepi-metadata RUNTIME DESTINATION bin)

enable_testing()

add_executable(test_metadata_parser
  tests/test-metadata-parser.cpp
  src/metadata-parser.cpp
  src/progress-parser.cpp
)
target_include_directories(test_metadata_parser PRIVATE include)
target_compile_options(test_metadata_parser PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME test_metadata_parser COMMAND test_metadata_parser)

add_executable(test_metadata_capture
  tests/test-metadata-capture.cpp
  src/metadata-parser.cpp
  src/progress-parser.cpp
)
target_include_directories(test_metadata_capture PRIVATE include)
target_compile_definitions(test_metadata_capture PRIVATE
  METADATA_CAPTURE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/metadata-capture-zone8.txt")
target_compile_options(test_metadata_capture PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME test_metadata_capture COMMAND test_metadata_capture)

add_executable(test_progress_parser tests/test-progress-parser.cpp src/progress-parser.cpp)
target_include_directories(test_progress_parser PRIVATE include)
target_compile_options(test_progress_parser PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME test_progress_parser COMMAND test_progress_parser)
