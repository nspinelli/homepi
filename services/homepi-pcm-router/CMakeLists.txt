cmake_minimum_required(VERSION 3.16)
project(homepi-pcm-router VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

set(SOURCES
  src/main.c
  src/config.c
  src/dac_resolver.c
  src/audio_loopback.c
  src/audio_router.c
  src/ringbuffer.c
  src/zone_state.c
  src/shairport_metadata_adapter.c
  src/json_events.c
  src/unix_socket_server.c
  src/mqtt_client.c
  src/log.c
)

add_executable(homepi-pcm-router ${SOURCES})
target_include_directories(homepi-pcm-router PRIVATE include)
target_link_libraries(homepi-pcm-router PRIVATE
  ${ALSA_LIBRARIES}
  ${MOSQUITTO_LIBRARIES}
  SQLite::SQLite3
  Threads::Threads
  m
)
target_compile_options(homepi-pcm-router PRIVATE -Wall -Wextra -Wpedantic)
target_link_directories(homepi-pcm-router PRIVATE ${ALSA_LIBRARY_DIRS} ${MOSQUITTO_LIBRARY_DIRS})
target_include_directories(homepi-pcm-router PRIVATE ${ALSA_INCLUDE_DIRS} ${MOSQUITTO_INCLUDE_DIRS})

install(TARGETS homepi-pcm-router RUNTIME DESTINATION bin)

enable_testing()

set(TEST_SOURCES
  tests/test_topic_parse.c
  tests/test_active_stack.c
  tests/test_pcm_mapping.c
  tests/test_fade_math.c
)

add_executable(test_topic_parse tests/test_topic_parse.c src/shairport_metadata_adapter.c)
target_include_directories(test_topic_parse PRIVATE include)
add_test(NAME test_topic_parse COMMAND test_topic_parse)

add_executable(test_pcm_mapping tests/test_pcm_mapping.c src/audio_loopback.c src/config.c src/log.c)
target_include_directories(test_pcm_mapping PRIVATE include)
target_link_libraries(test_pcm_mapping PRIVATE ${ALSA_LIBRARIES} m)
target_include_directories(test_pcm_mapping PRIVATE ${ALSA_INCLUDE_DIRS})
add_test(NAME test_pcm_mapping COMMAND test_pcm_mapping)

add_executable(test_active_stack tests/test_active_stack.c
  src/zone_state.c src/json_events.c src/unix_socket_server.c src/log.c)
target_include_directories(test_active_stack PRIVATE include)
target_link_libraries(test_active_stack PRIVATE Threads::Threads)
add_test(NAME test_active_stack COMMAND test_active_stack)

add_executable(test_fade_math tests/test_fade_math.c)
target_link_libraries(test_fade_math PRIVATE m)
add_test(NAME test_fade_math COMMAND test_fade_math)
