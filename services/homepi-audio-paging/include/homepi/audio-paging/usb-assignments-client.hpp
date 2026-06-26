#pragma once

#include <optional>
#include <string>

namespace homepi::audio_paging {

/** Read-only usb assignment snapshot returned by homepi-usb-devices. */
struct UsbAssignmentsSnapshot {
  std::optional<std::string> serial;
  std::optional<std::string> audio_primary;
  std::optional<std::string> paging;
};

/** Unix socket client for reading usb_assignments from homepi-usb-devices. */
class UsbAssignmentsClient {
 public:
  /** Creates a client for the usb-devices socket path. */
  explicit UsbAssignmentsClient(std::string socket_path);

  /** Reads current assignments via getAssignments request. */
  std::optional<UsbAssignmentsSnapshot> get_assignments() const;

 private:
  std::string socket_path_;
};

}  // namespace homepi::audio_paging
