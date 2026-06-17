#include "homepi/usb-devices/platform-loopback-policy.hpp"

namespace homepi::usb_devices {

homepi::storage::AudioProfileTuple PlatformLoopbackPolicy::default_loopback_profile() const {
  return homepi::storage::platform_loopback_default();
}

}  // namespace homepi::usb_devices
