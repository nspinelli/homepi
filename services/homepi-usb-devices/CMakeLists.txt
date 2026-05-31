cmake_minimum_required(VERSION 3.16)
project(homepi-usb-devices VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

set(HOMEPI_LOGGING_CPP "${CMAKE_CURRENT_SOURCE_DIR}/../../core/logging/cpp")

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
)

target_include_directories(homepi-usb-devices PRIVATE
  include
  ${HOMEPI_LOGGING_CPP}/include
  ${UDEV_INCLUDE_DIRS}
)

target_link_libraries(homepi-usb-devices PRIVATE
  ${UDEV_LIBRARIES}
  SQLite::SQLite3
  Threads::Threads
)

target_compile_options(homepi-usb-devices PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS homepi-usb-devices RUNTIME DESTINATION bin)
