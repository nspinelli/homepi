#include "homepi/audio-paging/service.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include "homepi/audio-paging/chime-player.hpp"
#include "homepi/audio-paging/dac-volume.hpp"
#include "homepi/audio-paging/events-subscriber.hpp"
#include "homepi/audio-paging/hifi-paging-controller.hpp"
#include "homepi/audio-paging/json-utils.hpp"
#include "homepi/audio-paging/orchestrator.hpp"
#include "homepi/audio-paging/paging-repository.hpp"
#include "homepi/audio-paging/pcm-player.hpp"
#include "homepi/audio-paging/piper-worker.hpp"
#include "homepi/audio-paging/resource-manager.hpp"
#include "homepi/audio-paging/unix-api-server.hpp"
#include "homepi/audio-paging/usb-assignments-client.hpp"
#include "homepi/events/event-emitter.hpp"
#include "homepi/events/events-client.hpp"

namespace fs = std::filesystem;

namespace homepi::audio_paging {

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

std::string read_file(const std::string& path) {
  std::ifstream input(path);
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

SpeakCommand parse_speak_command(const std::string& line) {
  SpeakCommand command;
  command.correlation_id = parse_correlation_id(line);
  const std::string payload = parse_payload_json(line);
  command.source = json_get_string(line, "source");
  command.text = json_get_string(payload, "text");
  command.voice_id = json_get_string(payload, "voiceId");
  command.include_chime = json_get_scalar(payload, "includeChime") == "true";
  command.chime_id = json_get_string(payload, "chimeId");
  const std::string on_busy = json_get_string(payload, "onBusy");
  if (!on_busy.empty()) {
    command.on_busy = on_busy;
  }
  const std::string wait_until = json_get_string(payload, "waitUntil");
  if (!wait_until.empty()) {
    command.wait_until = wait_until;
  }
  return command;
}

ChimeCommand parse_chime_command(const std::string& line) {
  ChimeCommand command;
  command.correlation_id = parse_correlation_id(line);
  command.source = json_get_string(line, "source");
  const std::string payload = parse_payload_json(line);
  command.chime_id = json_get_string(payload, "chimeId");
  return command;
}

PreviewVoiceCommand parse_preview_command(const std::string& line) {
  PreviewVoiceCommand command;
  command.correlation_id = parse_correlation_id(line);
  command.source = json_get_string(line, "source");
  const std::string payload = parse_payload_json(line);
  command.text = json_get_string(payload, "text");
  command.voice_id = json_get_string(payload, "voiceId");
  const std::string output = json_get_string(payload, "output");
  if (!output.empty()) {
    command.output = output;
  }
  return command;
}

ReloadVoiceCommand parse_reload_command(const std::string& line) {
  ReloadVoiceCommand command;
  command.correlation_id = parse_correlation_id(line);
  command.source = json_get_string(line, "source");
  const std::string payload = parse_payload_json(line);
  command.voice_id = json_get_string(payload, "voiceId");
  return command;
}

}  // namespace

Service::Service(ServiceConfig config) : config_(std::move(config)) {}

Service::~Service() { shutdown(); }

PagingStatus Service::build_status() const { return status_; }

void Service::refresh_readiness() {
  if (usb_client_ == nullptr) {
    return;
  }
  const auto assignments = usb_client_->get_assignments();
  status_.paging_dac_device_id =
      assignments && assignments->paging.has_value() ? *assignments->paging : "";
  status_.dac_connected = !status_.paging_dac_device_id.empty();
  status_.alsa_device = config_.alsa_device;
  status_.paging_alsa_card = -1;
  status_.dac_output_volume_percent = -1;
  if (status_.dac_connected) {
    if (const auto card_index = parse_alsa_card_index(status_.paging_dac_device_id)) {
      status_.paging_alsa_card = *card_index;
      if (pcm_player_ != nullptr) {
        pcm_player_->set_alsa_card_index(*card_index);
      }
      ensure_dac_playback_volume(*card_index);
      if (const auto volume = read_dac_playback_volume_percent(*card_index)) {
        status_.dac_output_volume_percent = *volume;
      }
    }
  } else if (pcm_player_ != nullptr) {
    pcm_player_->set_alsa_card_index(-1);
  }
  status_.voice_loaded = piper_worker_ != nullptr && piper_worker_->is_warm();
  status_.resource_state =
      resource_manager_ == nullptr ? ResourceState::Cold : resource_manager_->state();
  status_.busy = orchestrator_ != nullptr && orchestrator_->is_busy();
  status_.ready = status_.dac_connected && status_.hifi_connected && !status_.paging_dac_device_id.empty();
}

int Service::run() {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  std::error_code ec;
  fs::create_directories(fs::path(config_.database_path).parent_path(), ec);
  fs::create_directories(config_.voices_root, ec);
  fs::create_directories(config_.chimes_root, ec);
  fs::create_directories(config_.socket_dir, ec);

  const std::string migration_sql = read_file("storage/migrations/001-audio-paging.sql");
  repository_ = std::make_unique<PagingRepository>(config_.database_path, migration_sql);
  usb_client_ = std::make_unique<UsbAssignmentsClient>(config_.usb_devices_socket);
  piper_worker_ = std::make_unique<PiperWorker>(config_.piper_binary, config_.piper_default_voice,
                                                config_.piper_default_config);
  pcm_player_ = std::make_unique<PcmPlayer>(config_.alsa_device);
  chime_player_ = std::make_unique<ChimePlayer>(pcm_player_.get());
  resource_manager_ = std::make_unique<ResourceManager>();

  homepi::events::EventsClient publisher(config_.events_socket, config_.service);
  publisher.start({}, {"audio.paging.*", "modules.hifi.command"}, [](const std::string&) {});

  homepi::events::EventEmitter emitter(config_.service, [&publisher](const std::string& line) {
    publisher.publish(line);
  });

  hifi_controller_ = std::make_unique<HifiPagingController>(&emitter, config_.hifi_serial_socket);
  orchestrator_ = std::make_unique<Orchestrator>(
      repository_.get(), piper_worker_.get(), pcm_player_.get(), chime_player_.get(),
      hifi_controller_.get(), resource_manager_.get(), &emitter, config_.page_on_timeout_ms,
      config_.page_off_timeout_ms);

  pcm_player_->set_state_callbacks(
      [this, &emitter]() {
        status_.dac_open = true;
        emitter.emit("audio.paging", "audio.paging.dac.opened", "audio-paging", "{}");
      },
      [this, &emitter]() {
        status_.dac_open = false;
        emitter.emit("audio.paging", "audio.paging.dac.closed", "audio-paging", "{}");
      });

  refresh_readiness();
  status_.hifi_connected = true;
  const PagingConfig initial_config = repository_->get_config();
  resource_manager_->apply_config(initial_config);
  if (initial_config.enabled && initial_config.idle_policy == PagingIdlePolicy::AlwaysWarm) {
    if (const auto voice = repository_->get_voice(initial_config.active_voice_id)) {
      emitter.emit("audio.paging", "audio.paging.warming", "startup", "{}");
      piper_worker_->warm(voice->model_path, voice->config_path);
      resource_manager_->mark_warm();
      emitter.emit("audio.paging", "audio.paging.warm", "startup", "{}");
    }
  }

  refresh_readiness();
  emitter.emit("audio.paging", status_.ready ? "audio.paging.ready" : "audio.paging.not_ready",
               "startup", paging_status_to_json(status_));

  unix_api_ = std::make_unique<UnixApiServer>(
      config_.socket_path,
      UnixApiContext{
          .repository = repository_.get(),
          .status_fn = [this]() {
            refresh_readiness();
            return build_status();
          },
          .on_config_updated =
              [this, &emitter](const PagingConfig& config) {
                resource_manager_->apply_config(config);
                if (!config.enabled) {
                  piper_worker_->cool_down();
                  resource_manager_->force_cold();
                  emitter.emit("audio.paging", "audio.paging.disabled", "config", "{}");
                } else if (config.idle_policy == PagingIdlePolicy::AlwaysWarm &&
                           !piper_worker_->is_warm()) {
                  if (const auto voice = repository_->get_voice(config.active_voice_id)) {
                    emitter.emit("audio.paging", "audio.paging.warming", "config", "{}");
                    piper_worker_->warm(voice->model_path, voice->config_path);
                    resource_manager_->mark_warm();
                    emitter.emit("audio.paging", "audio.paging.warm", "config", "{}");
                  }
                }
                refresh_readiness();
              },
          .on_reload_voice =
              [this](const std::string& voice_id) {
                ReloadVoiceCommand command;
                command.correlation_id = "unix-api";
                command.voice_id = voice_id;
                return orchestrator_->handle_reload_voice(command);
              },
          .on_preview_voice =
              [this](const std::string& text, const std::string& voice_id) {
                PreviewVoiceCommand command;
                command.correlation_id = "unix-api";
                command.text = text.empty() ? "This is a HomePi paging preview." : text;
                command.voice_id = voice_id;
                return orchestrator_->handle_preview_voice(command);
              },
      });
  if (!unix_api_->start()) {
    publisher.stop();
    return 1;
  }

  events_subscriber_ = std::make_unique<EventsSubscriber>(
      config_.events_socket, config_.service,
      EventsSubscriberCallbacks{
          .on_speak =
              [this](const std::string& line) {
                refresh_readiness();
                orchestrator_->handle_speak(parse_speak_command(line), build_status());
              },
          .on_chime =
              [this](const std::string& line) {
                refresh_readiness();
                orchestrator_->handle_chime(parse_chime_command(line), build_status());
              },
          .on_preview_chime =
              [this](const std::string& line) {
                orchestrator_->handle_preview_chime(parse_chime_command(line));
              },
          .on_preview_voice =
              [this](const std::string& line) {
                orchestrator_->handle_preview_voice(parse_preview_command(line));
              },
          .on_reload_voice =
              [this](const std::string& line) {
                orchestrator_->handle_reload_voice(parse_reload_command(line));
              },
          .on_page_state_changed =
              [this](const std::string& line) {
                const std::string payload = parse_payload_json(line);
                const std::string page_state = json_get_scalar(payload, "page");
                if (!page_state.empty()) {
                  hifi_controller_->on_page_state_changed(std::stoi(page_state));
                }
              },
      });
  events_subscriber_->start();

  while (!g_stop.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const PagingConfig config = repository_->get_config();
    if (resource_manager_->should_cool_down(config)) {
      piper_worker_->cool_down();
      resource_manager_->force_cold();
      emitter.emit("audio.paging", "audio.paging.cold", "idle-timeout", "{}");
    }
    static int prune_counter = 0;
    if (++prune_counter >= 240) {
      prune_counter = 0;
      repository_->prune_old_jobs(30);
    }
    refresh_readiness();
  }

  shutdown();
  publisher.stop();
  return 0;
}

void Service::shutdown() {
  if (events_subscriber_ != nullptr) {
    events_subscriber_->stop();
    events_subscriber_.reset();
  }
  if (unix_api_ != nullptr) {
    unix_api_->stop();
    unix_api_.reset();
  }
}

}  // namespace homepi::audio_paging
