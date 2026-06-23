# CMake generated Testfile for 
# Source directory: /home/homepi/homepi/core/events/cpp
# Build directory: /home/homepi/homepi/services/homepi-pcm-router/build-user/homepi_core_events
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_event_emitter]=] "/home/homepi/homepi/services/homepi-pcm-router/build-user/homepi_core_events/test_event_emitter")
set_tests_properties([=[test_event_emitter]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/homepi/homepi/core/events/cpp/CMakeLists.txt;31;add_test;/home/homepi/homepi/core/events/cpp/CMakeLists.txt;0;")
subdirs("../homepi_core_transport")
