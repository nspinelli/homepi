# CMake generated Testfile for 
# Source directory: /home/homepi/homepi/services/homepi-pcm-router
# Build directory: /home/homepi/homepi/services/homepi-pcm-router/build-user
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_routing_state]=] "/home/homepi/homepi/services/homepi-pcm-router/build-user/test_routing_state")
set_tests_properties([=[test_routing_state]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/homepi/homepi/services/homepi-pcm-router/CMakeLists.txt;56;add_test;/home/homepi/homepi/services/homepi-pcm-router/CMakeLists.txt;0;")
subdirs("homepi_core_storage")
subdirs("homepi_core_events")
