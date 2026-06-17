#pragma once

#include <functional>
#include <string>

#include "homepi/events/event-emitter.hpp"

namespace homepi::usb_devices {

/** Broadcasts usb-devices module events through core/events. */
class UsbEventEmitter {
 public:
  using BroadcastFn = std::function<void(const std::string& ndjson_line)>;

  /**
   * Creates an emitter bound to a broadcast sink.
   * @param broadcast Socket broadcast callback.
   */
  explicit UsbEventEmitter(BroadcastFn broadcast);

  void emit_audio_capabilities_probed(const std::string& correlation_id,
                                      const std::string& payload_json);
  void emit_audio_operating_profile_changed(const std::string& correlation_id,
                                            const std::string& payload_json);
  void emit_primary_audio_unassigned(const std::string& correlation_id);
  void emit_audio_profile_invalid(const std::string& correlation_id,
                                  const std::string& payload_json);
  void emit_audio_profile_paused(const std::string& correlation_id);

 private:
  homepi::events::EventEmitter emitter_;
};

}  // namespace homepi::usb_devices
