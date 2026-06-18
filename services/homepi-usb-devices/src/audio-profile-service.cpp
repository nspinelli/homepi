#include "homepi/usb-devices/audio-profile-service.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "homepi/storage/audio-profile-repository.hpp"
#include "homepi/storage/database-connection.hpp"
#include "homepi/usb-devices/audio-profile-validator.hpp"
#include "homepi/usb-devices/json-utils.hpp"

namespace fs = std::filesystem;

namespace homepi::usb_devices {

namespace {

std::string tuple_json(const homepi::storage::AudioProfileTuple& tuple) {
  std::ostringstream out;
  out << "{\"sampleRate\":" << tuple.sample_rate << ",\"channels\":" << tuple.channels
      << ",\"sampleFormat\":\"" << homepi::storage::sample_format_to_string(tuple.sample_format)
      << "\"}";
  return out.str();
}

}  // namespace

AudioProfileService::AudioProfileService(AssignmentRepository& repository,
                                         AudioProfileWriter& writer, AlsaCapabilityProbe& probe,
                                         UsbEventEmitter& events, std::string database_path,
                                         std::string generated_dir)
    : repository_(repository),
      writer_(writer),
      probe_(probe),
      events_(events),
      database_path_(std::move(database_path)),
      generated_dir_(std::move(generated_dir)) {}

std::optional<homepi::storage::AudioCapabilities> AudioProfileService::load_capabilities(
    const std::string& device_id) const {
  if (!fs::exists(database_path_)) {
    return std::nullopt;
  }
  homepi::storage::DatabaseConnection db(database_path_, homepi::storage::DatabaseOpenMode::ReadOnly);
  homepi::storage::AudioProfileRepository repo(db);
  const auto direct = repo.get_capabilities(device_id);
  if (direct.has_value() && !direct->supported_profile_tuples.empty()) {
    return direct;
  }
  const auto inherited = repo.get_capabilities_for_identity(device_id);
  if (!inherited.has_value() || inherited->supported_profile_tuples.empty()) {
    return direct;
  }
  homepi::storage::AudioCapabilities resolved = *inherited;
  resolved.device_id = device_id;
  return resolved;
}

void AudioProfileService::refresh_audio_capabilities(const std::vector<UsbDevice>& devices) {
  for (const UsbDevice& device : devices) {
    if (device.kind != DeviceKind::Audio || !device.present) {
      continue;
    }
    std::optional<homepi::storage::AudioCapabilities> capabilities =
        probe_.probe_playback(device.device_id, device.resolved_alsa_name);
    if (!capabilities.has_value() || capabilities->supported_profile_tuples.empty()) {
      capabilities = load_capabilities(device.device_id);
    }
    if (!capabilities.has_value() || capabilities->supported_profile_tuples.empty()) {
      continue;
    }
    capabilities->device_id = device.device_id;
    writer_.upsert_capabilities(*capabilities);
    std::ostringstream payload;
    payload << "{\"deviceId\":\"" << json_escape(device.device_id) << "\",\"tupleCount\":"
            << capabilities->supported_profile_tuples.size() << "}";
    events_.emit_audio_capabilities_probed("hotplug", payload.str());
  }
}

void AudioProfileService::validate_active_profile(const UsbAssignments& assignments,
                                                  const std::string& correlation_id) {
  if (!assignments.audio_primary || assignments.audio_primary->empty() ||
      !assignments.audio_primary_profile.has_value()) {
    return;
  }

  const auto capabilities = load_capabilities(*assignments.audio_primary);
  if (!capabilities.has_value() ||
      !AudioProfileValidator::tuple_supported(*capabilities, *assignments.audio_primary_profile)) {
    writer_.mark_profile_paused_invalid();
    std::ostringstream payload;
    payload << "{\"deviceId\":\"" << json_escape(*assignments.audio_primary) << "\"}";
    events_.emit_audio_profile_invalid(correlation_id, payload.str());
    events_.emit_audio_profile_paused(correlation_id);
  }
}

bool AudioProfileService::apply_assignments(const UsbAssignments& assignments,
                                            const std::vector<UsbDevice>& devices,
                                            const std::string& correlation_id,
                                            std::string& error_out) {
  std::optional<homepi::storage::AudioCapabilities> capabilities;
  if (assignments.audio_primary && !assignments.audio_primary->empty()) {
    const auto device = repository_.get_device(*assignments.audio_primary);
    if (!device.has_value()) {
      error_out = "Unknown device for audioPrimary";
      return false;
    }
    const auto probed = probe_.probe_playback(device->device_id, device->resolved_alsa_name);
    if (probed.has_value()) {
      writer_.upsert_capabilities(*probed);
      capabilities = *probed;
    } else {
      capabilities = load_capabilities(*assignments.audio_primary);
    }
  }

  const std::string profile_error =
      AudioProfileValidator::validate_assignment_profile(assignments, capabilities);
  if (!profile_error.empty()) {
    error_out = profile_error;
    return false;
  }

  if (!repository_.set_assignments(assignments, devices, error_out)) {
    return false;
  }

  if (!assignments.audio_primary || assignments.audio_primary->empty()) {
    const uint64_t revision = writer_.apply_platform_policy();
    std::ostringstream payload;
    payload << "{\"profileRevision\":" << revision << ",\"profileMode\":\"no_dac_assigned\"}";
    events_.emit_primary_audio_unassigned(correlation_id);
    events_.emit_audio_operating_profile_changed(correlation_id, payload.str());
    return true;
  }

  const auto device = repository_.get_device(*assignments.audio_primary);
  if (!device.has_value() || !assignments.audio_primary_profile.has_value()) {
    error_out = "Primary audio device or profile missing after save";
    return false;
  }

  const uint64_t revision =
      writer_.apply_primary_profile(*device, *assignments.audio_primary_profile);
  std::ostringstream payload;
  payload << "{\"profileRevision\":" << revision << ",\"profileMode\":\"dac_assigned\","
          << "\"loopbackProfile\":" << tuple_json(*assignments.audio_primary_profile) << "}";
  events_.emit_audio_operating_profile_changed(correlation_id, payload.str());
  return true;
}

std::string AudioProfileService::capabilities_json(const std::string& device_id) const {
  const auto capabilities = load_capabilities(device_id);
  std::ostringstream out;
  out << "{\"deviceId\":\"" << json_escape(device_id) << "\",\"supportedProfileTuples\":[";
  if (capabilities.has_value()) {
    for (std::size_t i = 0; i < capabilities->supported_profile_tuples.size(); ++i) {
      if (i > 0) {
        out << ',';
      }
      out << tuple_json(capabilities->supported_profile_tuples[i]);
    }
    out << "],\"probedAt\":\"" << json_escape(capabilities->probed_at) << "\"";
    if (capabilities->probe_error.has_value()) {
      out << ",\"probeError\":\"" << json_escape(*capabilities->probe_error) << "\"";
    }
  } else {
    out << "]";
  }
  out << '}';
  return out.str();
}

std::string AudioProfileService::operating_profile_json() const {
  const fs::path artifact_path = fs::path(generated_dir_) / "audio" / "operating-profile.json";
  std::ifstream input(artifact_path);
  if (!input.is_open()) {
    return "{\"mode\":\"no_dac_assigned\"}";
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

}  // namespace homepi::usb_devices
