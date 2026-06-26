#include <iostream>
#include <string>

#include "homepi/audio-paging/service-config.hpp"
#include "homepi/audio-paging/service.hpp"

int main(int argc, char** argv) {
  const std::string config_path =
      argc > 1 ? argv[1] : "/opt/homepi/services/audio-paging/config/service-config.json";
  try {
    const auto config = homepi::audio_paging::load_service_config(config_path);
    homepi::audio_paging::Service service(config);
    return service.run();
  } catch (const std::exception& ex) {
    std::cerr << "homepi-audio-paging failed: " << ex.what() << "\n";
    return 1;
  }
}
