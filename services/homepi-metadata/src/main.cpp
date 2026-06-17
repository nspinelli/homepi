#include "homepi/metadata/service.hpp"
#include "homepi/metadata/service-config.hpp"

int main() {
  homepi::metadata::Service service(homepi::metadata::load_config_from_env());
  return service.run();
}
