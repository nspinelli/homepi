#pragma once

#include <string>

#include "homepi/storage/audio-profile-types.hpp"
#include "homepi/usb-devices/types.hpp"

struct sqlite3;

namespace homepi::usb_devices {

/** Persists capabilities and operating profiles via core storage tables. */
class AudioProfileWriter {
 public:
  /**
   * Creates a writer for the given sqlite connection.
   * @param db SQLite database handle.
   * @param generated_dir Generated artifact root directory.
   * @param primary_alsa_id Stable primary ALSA card id.
   */
  AudioProfileWriter(sqlite3* db, std::string generated_dir, std::string primary_alsa_id);

  /**
   * Upserts probed capabilities and tuples for a device.
   * @param capabilities Probed capabilities.
   */
  void upsert_capabilities(const homepi::storage::AudioCapabilities& capabilities);

  /**
   * Applies platform loopback policy when no DAC is assigned.
   * @return New profile revision.
   */
  uint64_t apply_platform_policy();

  /**
   * Applies a user-selected primary audio profile.
   * @param device Assigned device metadata.
   * @param tuple User-selected tuple.
   * @return New profile revision.
   */
  uint64_t apply_primary_profile(const UsbDevice& device,
                                 const homepi::storage::AudioProfileTuple& tuple);

  /**
   * Marks the active profile paused/invalid.
   * @return New profile revision.
   */
  uint64_t mark_profile_paused_invalid();

  /**
   * Returns the current profile revision.
   * @return Revision counter.
   */
  uint64_t current_revision() const;

 private:
  void write_artifact(const homepi::storage::ActiveAudioConfig& config);
  uint64_t bump_revision(const std::string& status);
  std::string stable_primary_hw() const;

  sqlite3* db_;
  std::string generated_dir_;
  std::string primary_alsa_id_;
};

}  // namespace homepi::usb_devices
