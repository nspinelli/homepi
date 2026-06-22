cmake_minimum_required(VERSION 3.20)
project(homepi_core_transport LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(homepi_core_transport STATIC
  src/unix-socket.cpp
  src/latest-value-publisher.cpp
)

target_include_directories(homepi_core_transport PUBLIC include)
target_compile_options(homepi_core_transport PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()

# TODO: fix socketpair flush timing in test_latest_value_publisher
# add_executable(test_latest_value_publisher tests/test-latest-value-publisher.cpp)
# target_link_libraries(test_latest_value_publisher PRIVATE homepi_core_transport)
# add_test(NAME test_latest_value_publisher COMMAND test_latest_value_publisher)
