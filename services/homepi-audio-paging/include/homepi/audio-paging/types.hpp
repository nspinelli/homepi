#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace homepi::audio_paging {

/** Supported paging idle policies for Piper worker lifecycle. */
enum class PagingIdlePolicy { AlwaysWarm, WarmWithTimeout };

/** Runtime resource state for paging worker and playback resources. */
enum class ResourceState { Disabled, Cold, Warm, Active };

/** Persistent service configuration row loaded from SQLite. */
struct PagingConfig {
  bool enabled = false;
  std::string default_voice_id = "en_US-lessac-medium";
  std::string active_voice_id = "en_US-lessac-medium";
  std::string active_chime_id = "default";
  int max_installed_voices = 2;
  int max_text_length = 4000;
  int max_preview_text_length = 120;
  int stream_threshold_chars = 200;
  int min_preroll_ms = 100;
  std::string default_on_busy = "reject";
  PagingIdlePolicy idle_policy = PagingIdlePolicy::AlwaysWarm;
  int idle_warm_timeout_ms = 1800000;
  int dac_idle_close_delay_ms = 3000;
  bool keep_last_audio_for_debug = false;
  std::string created_at;
  std::string updated_at;
};

/** Installed Piper voice metadata row. */
struct PagingVoice {
  std::string voice_id;
  std::string display_name;
  std::string language_code;
  std::string quality;
  std::string model_path;
  std::string config_path;
  bool installed = true;
  bool is_default = false;
  bool is_bundled = false;
  std::string last_used_at;
  std::string created_at;
  std::string updated_at;
};

/** Chime metadata row used for optional pre-speech playback. */
struct PagingChime {
  std::string chime_id;
  std::string display_name;
  std::string file_path;
  int duration_ms = 0;
  bool is_default = false;
  bool is_bundled = false;
  std::string created_at;
  std::string updated_at;
};

/** API key metadata stored in SQLite. */
struct PagingApiKeyMetadata {
  bool configured = false;
  std::string prefix;
  std::string hash;
};

/** In-memory readiness and health status for unix API responses. */
struct PagingStatus {
  bool ready = false;
  ResourceState resource_state = ResourceState::Cold;
  bool dac_connected = false;
  bool dac_open = false;
  bool voice_loaded = false;
  bool hifi_connected = false;
  bool busy = false;
  std::string paging_dac_device_id;
  std::string alsa_device = "plug:AudioPaging";
  int paging_alsa_card = -1;
  int dac_output_volume_percent = -1;
};

/** Speak request payload from broker command. */
struct SpeakCommand {
  std::string correlation_id;
  std::string source = "unknown";
  std::string text;
  std::string voice_id;
  bool include_chime = false;
  std::string chime_id;
  std::string on_busy = "reject";
  std::string wait_until = "accepted";
};

/** Chime-only request payload from broker command. */
struct ChimeCommand {
  std::string correlation_id;
  std::string source = "unknown";
  std::string chime_id;
  std::string on_busy = "reject";
  std::string wait_until = "accepted";
};

/** Voice preview request payload from broker command. */
struct PreviewVoiceCommand {
  std::string correlation_id;
  std::string source = "unknown";
  std::string text;
  std::string voice_id;
  std::string output = "paging_dac_only";
};

/** Explicit voice reload command payload from broker command. */
struct ReloadVoiceCommand {
  std::string correlation_id;
  std::string source = "unknown";
  std::string voice_id;
};

/** Parsed static voice catalog entry shipped with the service. */
struct CatalogVoice {
  std::string voice_id;
  std::string display_name;
  std::string language_code;
  std::string quality;
  std::string sample_url;
};

}  // namespace homepi::audio_paging
