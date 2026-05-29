#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace homepi::usb_devices {

/** Callback invoked on hotplug with action "add" or "remove". */
using UdevHotplugCallback = std::function<void(const std::string& action, const std::string& devpath)>;

/** Background libudev monitor for USB hotplug events. */
class UdevMonitor {
 public:
  UdevMonitor();
  ~UdevMonitor();

  UdevMonitor(const UdevMonitor&) = delete;
  UdevMonitor& operator=(const UdevMonitor&) = delete;

  /**
   * Starts the monitor thread.
   * @param callback Hotplug callback.
   * @return True when started.
   */
  bool start(UdevHotplugCallback callback);

  /** Stops the monitor thread. */
  void stop();

  /**
   * Returns whether the monitor thread is active.
   * @return Active flag.
   */
  bool active() const { return active_.load(); }

 private:
  void run();

  UdevHotplugCallback callback_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> active_{false};
};

}  // namespace homepi::usb_devices
