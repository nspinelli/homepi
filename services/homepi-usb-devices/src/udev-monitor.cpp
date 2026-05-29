#include "homepi/usb-devices/udev-monitor.hpp"

#include <libudev.h>
#include <poll.h>

namespace homepi::usb_devices {

UdevMonitor::UdevMonitor() = default;

UdevMonitor::~UdevMonitor() { stop(); }

bool UdevMonitor::start(UdevHotplugCallback callback) {
  if (active_.load()) {
    return true;
  }
  callback_ = std::move(callback);
  stop_ = false;
  thread_ = std::thread([this]() { run(); });
  return true;
}

void UdevMonitor::stop() {
  stop_ = true;
  if (thread_.joinable()) {
    thread_.join();
  }
  active_ = false;
}

void UdevMonitor::run() {
  udev* udev_ctx = udev_new();
  if (udev_ctx == nullptr) {
    return;
  }

  udev_monitor* mon = udev_monitor_new_from_netlink(udev_ctx, "udev");
  if (mon == nullptr) {
    udev_unref(udev_ctx);
    return;
  }

  udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", nullptr);
  udev_monitor_enable_receiving(mon);
  const int fd = udev_monitor_get_fd(mon);
  active_ = true;

  while (!stop_.load()) {
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    const int ready = poll(&pfd, 1, 500);
    if (ready <= 0) {
      continue;
    }

    udev_device* dev = udev_monitor_receive_device(mon);
    if (dev == nullptr) {
      continue;
    }

    const char* action = udev_device_get_action(dev);
    const char* devpath = udev_device_get_devpath(dev);
    if (action != nullptr && devpath != nullptr && callback_) {
      callback_(action, devpath);
    }
    udev_device_unref(dev);
  }

  udev_monitor_unref(mon);
  udev_unref(udev_ctx);
  active_ = false;
}

}  // namespace homepi::usb_devices
