cmake_minimum_required(VERSION 3.20)
project(homepi_core_events LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HOMEPI_TRANSPORT_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../transport/cpp")
add_subdirectory(${HOMEPI_TRANSPORT_CPP} ${CMAKE_BINARY_DIR}/homepi_core_transport)

add_library(homepi_core_events STATIC
  src/broker-protocol.cpp
  src/event-envelope.cpp
  src/event-emitter.cpp
  src/events-client.cpp
)

target_include_directories(homepi_core_events PUBLIC include)
target_link_libraries(homepi_core_events PUBLIC homepi_core_transport)
target_compile_options(homepi_core_events PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()

add_executable(test_event_emitter tests/test-event-emitter.cpp)
target_link_libraries(test_event_emitter PRIVATE homepi_core_events)
add_test(NAME test_event_emitter COMMAND test_event_emitter)
