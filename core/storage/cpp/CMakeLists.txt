cmake_minimum_required(VERSION 3.20)
project(homepi_core_storage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)

add_library(homepi_core_storage STATIC
  src/database-connection.cpp
  src/migration-runner.cpp
  src/audio-profile-repository.cpp
)

target_include_directories(homepi_core_storage PUBLIC include)
target_link_libraries(homepi_core_storage PUBLIC SQLite::SQLite3)
target_compile_options(homepi_core_storage PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()

add_executable(test_audio_profile_repository tests/test-audio-profile-repository.cpp)
target_link_libraries(test_audio_profile_repository PRIVATE homepi_core_storage)
add_test(NAME test_audio_profile_repository COMMAND test_audio_profile_repository)
