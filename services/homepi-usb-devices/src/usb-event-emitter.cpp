#include "homepi/usb-devices/usb-event-emitter.hpp"

namespace homepi::usb_devices {

UsbEventEmitter::UsbEventEmitter(BroadcastFn broadcast)
    : emitter_("homepi-usb-devices", std::move(broadcast)) {}

void UsbEventEmitter::emit_audio_capabilities_probed(const std::string& correlation_id,
                                                   const std::string& payload_json) {
  emitter_.emit("modules.usb.audio", "audio_capabilities_probed", correlation_id, payload_json);
}

void UsbEventEmitter::emit_audio_operating_profile_changed(const std::string& correlation_id,
                                                          const std::string& payload_json) {
  emitter_.emit("modules.usb.audio", "audio_operating_profile_changed", correlation_id,
                payload_json);
}

void UsbEventEmitter::emit_primary_audio_unassigned(const std::string& correlation_id) {
  emitter_.emit("modules.usb.audio", "primary_audio_unassigned", correlation_id, "{}");
}

void UsbEventEmitter::emit_audio_profile_invalid(const std::string& correlation_id,
                                                 const std::string& payload_json) {
  emitter_.emit("modules.usb.audio", "audio_profile_invalid", correlation_id, payload_json);
}

void UsbEventEmitter::emit_audio_profile_paused(const std::string& correlation_id) {
  emitter_.emit("modules.usb.audio", "audio_profile_paused", correlation_id, "{}");
}

}  // namespace homepi::usb_devices
