#include "homepi/audio-paging/unix-api-server.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <sstream>

#include "homepi/audio-paging/json-utils.hpp"
#include "homepi/audio-paging/paging-repository.hpp"

namespace fs = std::filesystem;

namespace homepi::audio_paging {

namespace {

std::string voice_to_json(const PagingVoice& voice) {
  std::ostringstream out;
  out << "{"
      << "\"voiceId\":\"" << json_escape(voice.voice_id) << "\","
      << "\"displayName\":\"" << json_escape(voice.display_name) << "\","
      << "\"languageCode\":\"" << json_escape(voice.language_code) << "\","
      << "\"quality\":\"" << json_escape(voice.quality) << "\","
      << "\"modelPath\":\"" << json_escape(voice.model_path) << "\","
      << "\"configPath\":\"" << json_escape(voice.config_path) << "\","
      << "\"installed\":" << (voice.installed ? "true" : "false") << ","
      << "\"isDefault\":" << (voice.is_default ? "true" : "false") << ","
      << "\"isBundled\":" << (voice.is_bundled ? "true" : "false") << "}";
  return out.str();
}

std::string chime_to_json(const PagingChime& chime) {
  std::ostringstream out;
  out << "{"
      << "\"chimeId\":\"" << json_escape(chime.chime_id) << "\","
      << "\"displayName\":\"" << json_escape(chime.display_name) << "\","
      << "\"filePath\":\"" << json_escape(chime.file_path) << "\","
      << "\"durationMs\":" << chime.duration_ms << ","
      << "\"isDefault\":" << (chime.is_default ? "true" : "false") << ","
      << "\"isBundled\":" << (chime.is_bundled ? "true" : "false") << "}";
  return out.str();
}

std::string config_to_json(const PagingConfig& config, const PagingStatus& status) {
  std::ostringstream out;
  out << "{"
      << "\"enabled\":" << (config.enabled ? "true" : "false") << ","
      << "\"pagingDacDeviceId\":";
  if (status.paging_dac_device_id.empty()) {
    out << "null";
  } else {
    out << "\"" << json_escape(status.paging_dac_device_id) << "\"";
  }
  out << ",\"pagingDacSource\":\"usb_assignments.paging\","
      << "\"defaultVoiceId\":\"" << json_escape(config.default_voice_id) << "\","
      << "\"activeVoiceId\":\"" << json_escape(config.active_voice_id) << "\","
      << "\"activeChimeId\":\"" << json_escape(config.active_chime_id) << "\","
      << "\"maxInstalledVoices\":" << config.max_installed_voices << ","
      << "\"maxTextLength\":" << config.max_text_length << ","
      << "\"maxPreviewTextLength\":" << config.max_preview_text_length << ","
      << "\"streamThresholdChars\":" << config.stream_threshold_chars << ","
      << "\"defaultOnBusy\":\"" << json_escape(config.default_on_busy) << "\","
      << "\"idlePolicy\":\"" << idle_policy_to_string(config.idle_policy) << "\","
      << "\"idleWarmTimeoutMs\":" << config.idle_warm_timeout_ms << ","
      << "\"dacIdleCloseDelayMs\":" << config.dac_idle_close_delay_ms << ","
      << "\"keepLastAudioForDebug\":" << (config.keep_last_audio_for_debug ? "true" : "false")
      << ",\"status\":" << paging_status_to_json(status) << "}";
  return out.str();
}

}  // namespace

UnixApiServer::UnixApiServer(std::string socket_path, UnixApiContext context)
    : socket_path_(std::move(socket_path)), context_(std::move(context)) {
  const auto slash = socket_path_.find_last_of('/');
  socket_dir_ = slash == std::string::npos ? "/run/homepi" : socket_path_.substr(0, slash);
}

UnixApiServer::~UnixApiServer() { stop(); }

bool UnixApiServer::start() {
  if (server_fd_ >= 0) {
    return true;
  }
  std::error_code ec;
  fs::create_directories(socket_dir_, ec);
  fs::remove(socket_path_, ec);
  server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    return false;
  }
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (socket_path_.size() >= sizeof(addr.sun_path)) {
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }
  std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
  if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }
  if (listen(server_fd_, 16) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }
  stop_ = false;
  server_thread_ = std::thread([this]() { listen_loop(); });
  return true;
}

void UnixApiServer::stop() {
  stop_ = true;
  if (server_fd_ >= 0) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
  std::lock_guard<std::mutex> lock(clients_mutex_);
  for (const int fd : subscribers_) {
    close(fd);
  }
  subscribers_.clear();
  std::error_code ec;
  fs::remove(socket_path_, ec);
}

void UnixApiServer::broadcast(const std::string& line) {
  const std::string frame = line + "\n";
  std::lock_guard<std::mutex> lock(clients_mutex_);
  for (auto it = subscribers_.begin(); it != subscribers_.end();) {
    if (write(*it, frame.c_str(), frame.size()) < 0) {
      close(*it);
      it = subscribers_.erase(it);
    } else {
      ++it;
    }
  }
}

void UnixApiServer::listen_loop() {
  while (!stop_.load()) {
    const int client = accept(server_fd_, nullptr, nullptr);
    if (client < 0) {
      if (stop_.load()) {
        break;
      }
      continue;
    }
    std::thread([this, client]() { handle_client(client); }).detach();
  }
}

void UnixApiServer::handle_client(int fd) {
  std::string buffer;
  char chunk[4096];
  bool subscribed = false;
  while (!stop_.load()) {
    const ssize_t size = read(fd, chunk, sizeof(chunk));
    if (size <= 0) {
      break;
    }
    buffer.append(chunk, static_cast<std::size_t>(size));
    std::size_t newline = 0;
    while ((newline = buffer.find('\n')) != std::string::npos) {
      const std::string line = buffer.substr(0, newline);
      buffer.erase(0, newline + 1);
      if (line.empty()) {
        continue;
      }
      const std::string method = json_get_string(line, "method");
      if (method == "subscribe") {
        subscribed = true;
        std::lock_guard<std::mutex> lock(clients_mutex_);
        subscribers_.insert(fd);
      }
      const std::string response = handle_request(line) + "\n";
      write(fd, response.c_str(), response.size());
      if (!subscribed) {
        break;
      }
    }
    if (!subscribed) {
      break;
    }
  }
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    subscribers_.erase(fd);
  }
  close(fd);
}

std::string UnixApiServer::ok_response(const std::string& correlation_id,
                                       const std::string& data_json) const {
  const std::string resolved_id = correlation_id.empty() ? "audio-paging" : correlation_id;
  return "{\"ok\":true,\"correlationId\":\"" + json_escape(resolved_id) + "\",\"data\":" + data_json +
         "}";
}

std::string UnixApiServer::error_response(const std::string& correlation_id, const std::string& code,
                                          const std::string& message) const {
  const std::string resolved_id = correlation_id.empty() ? "audio-paging" : correlation_id;
  return "{\"ok\":false,\"correlationId\":\"" + json_escape(resolved_id) + "\",\"error\":{\"code\":\"" +
         json_escape(code) + "\",\"message\":\"" + json_escape(message) + "\"}}";
}

std::string UnixApiServer::handle_request(const std::string& line) {
  const std::string method = json_get_string(line, "method");
  const std::string correlation_id = json_get_string(line, "correlationId");
  if (context_.repository == nullptr) {
    return error_response(correlation_id, "PAGING_REPOSITORY_UNAVAILABLE", "Repository unavailable");
  }
  if (method == "subscribe") {
    return ok_response(correlation_id, "{\"subscribed\":true}");
  }
  if (method == "getHealth") {
    const PagingStatus status = context_.status_fn ? context_.status_fn() : PagingStatus{};
    return ok_response(correlation_id, paging_status_to_json(status));
  }
  if (method == "getStatus") {
    const PagingStatus status = context_.status_fn ? context_.status_fn() : PagingStatus{};
    return ok_response(correlation_id, paging_status_to_json(status));
  }
  if (method == "getConfig") {
    const PagingConfig config = context_.repository->get_config();
    const PagingStatus status = context_.status_fn ? context_.status_fn() : PagingStatus{};
    return ok_response(correlation_id, config_to_json(config, status));
  }
  if (method == "updateConfig") {
    PagingConfig config = context_.repository->get_config();
    const std::string enabled = json_get_scalar(line, "enabled");
    if (!enabled.empty()) {
      config.enabled = enabled == "true" || enabled == "1";
    }
    const std::string default_voice = json_get_string(line, "defaultVoiceId");
    if (!default_voice.empty()) {
      config.default_voice_id = default_voice;
      config.active_voice_id = default_voice;
    }
    const std::string active_chime = json_get_string(line, "activeChimeId");
    if (!active_chime.empty()) {
      config.active_chime_id = active_chime;
    }
    const std::string idle_policy = json_get_string(line, "idlePolicy");
    if (!idle_policy.empty()) {
      config.idle_policy = parse_idle_policy(idle_policy, config.idle_policy);
    }
    const std::string idle_timeout = json_get_scalar(line, "idleWarmTimeoutMs");
    if (!idle_timeout.empty()) {
      config.idle_warm_timeout_ms = std::stoi(idle_timeout);
    }
    const std::string dac_delay = json_get_scalar(line, "dacIdleCloseDelayMs");
    if (!dac_delay.empty()) {
      config.dac_idle_close_delay_ms = std::stoi(dac_delay);
    }
    if (!context_.repository->update_config(config)) {
      return error_response(correlation_id, "PAGING_CONFIG_UPDATE_FAILED", "Failed to update config");
    }
    if (context_.on_config_updated) {
      context_.on_config_updated(config);
    }
    const PagingStatus status = context_.status_fn ? context_.status_fn() : PagingStatus{};
    return ok_response(correlation_id, config_to_json(config, status));
  }
  if (method == "getVoices") {
    const auto voices = context_.repository->list_voices();
    std::ostringstream data;
    data << "{\"voices\":[";
    for (std::size_t i = 0; i < voices.size(); ++i) {
      if (i > 0) {
        data << ",";
      }
      data << voice_to_json(voices[i]);
    }
    data << "]}";
    return ok_response(correlation_id, data.str());
  }
  if (method == "getChimes") {
    const auto chimes = context_.repository->list_chimes();
    std::ostringstream data;
    data << "{\"chimes\":[";
    for (std::size_t i = 0; i < chimes.size(); ++i) {
      if (i > 0) {
        data << ",";
      }
      data << chime_to_json(chimes[i]);
    }
    data << "]}";
    return ok_response(correlation_id, data.str());
  }
  if (method == "setActiveVoice") {
    const std::string voice_id = json_get_string(line, "voiceId");
    if (voice_id.empty()) {
      return error_response(correlation_id, "PAGING_VALIDATION_ERROR", "voiceId is required");
    }
    if (!context_.repository->set_active_voice(voice_id)) {
      return error_response(correlation_id, "PAGING_VOICE_UPDATE_FAILED", "Failed to set active voice");
    }
    return ok_response(correlation_id, "{\"voiceId\":\"" + json_escape(voice_id) + "\"}");
  }
  if (method == "installVoice") {
    const std::string voice_id = json_get_string(line, "voiceId");
    const std::string display_name = json_get_string(line, "displayName");
    const std::string language_code = json_get_string(line, "languageCode");
    const std::string quality = json_get_string(line, "quality");
    const std::string model_path = json_get_string(line, "modelPath");
    const std::string config_path = json_get_string(line, "configPath");
    if (voice_id.empty() || model_path.empty() || config_path.empty()) {
      return error_response(correlation_id, "PAGING_VALIDATION_ERROR",
                            "voiceId, modelPath, and configPath are required");
    }
    PagingVoice voice;
    voice.voice_id = voice_id;
    voice.display_name = display_name.empty() ? voice_id : display_name;
    voice.language_code = language_code.empty() ? "en-US" : language_code;
    voice.quality = quality;
    voice.model_path = model_path;
    voice.config_path = config_path;
    voice.installed = true;
    if (!context_.repository->upsert_voice(voice)) {
      return error_response(correlation_id, "PAGING_VOICE_INSTALL_FAILED", "Failed to install voice");
    }
    return ok_response(correlation_id, "{\"voiceId\":\"" + json_escape(voice_id) + "\"}");
  }
  if (method == "removeVoice") {
    const std::string voice_id = json_get_string(line, "voiceId");
    if (voice_id.empty()) {
      return error_response(correlation_id, "PAGING_VALIDATION_ERROR", "voiceId is required");
    }
    if (!context_.repository->remove_voice(voice_id)) {
      const PagingConfig config = context_.repository->get_config();
      if (voice_id == config.active_voice_id) {
        return error_response(correlation_id, "PAGING_VOICE_REMOVE_FAILED",
                              "Cannot remove the active default voice");
      }
      return error_response(correlation_id, "PAGING_VOICE_REMOVE_FAILED", "Failed to remove voice");
    }
    return ok_response(correlation_id, "{\"voiceId\":\"" + json_escape(voice_id) + "\"}");
  }
  if (method == "setActiveChime") {
    const std::string chime_id = json_get_string(line, "chimeId");
    if (chime_id.empty()) {
      return error_response(correlation_id, "PAGING_VALIDATION_ERROR", "chimeId is required");
    }
    if (!context_.repository->set_active_chime(chime_id)) {
      return error_response(correlation_id, "PAGING_CHIME_UPDATE_FAILED", "Failed to set active chime");
    }
    return ok_response(correlation_id, "{\"chimeId\":\"" + json_escape(chime_id) + "\"}");
  }
  if (method == "uploadChime") {
    const std::string chime_id = json_get_string(line, "chimeId");
    const std::string display_name = json_get_string(line, "displayName");
    const std::string file_path = json_get_string(line, "filePath");
    const std::string duration_ms = json_get_scalar(line, "durationMs");
    if (chime_id.empty() || file_path.empty()) {
      return error_response(correlation_id, "PAGING_VALIDATION_ERROR",
                            "chimeId and filePath are required");
    }
    PagingChime chime;
    chime.chime_id = chime_id;
    chime.display_name = display_name.empty() ? chime_id : display_name;
    chime.file_path = file_path;
    if (!duration_ms.empty()) {
      chime.duration_ms = std::stoi(duration_ms);
    }
    if (!context_.repository->upsert_chime(chime)) {
      return error_response(correlation_id, "PAGING_CHIME_UPLOAD_FAILED", "Failed to save chime");
    }
    return ok_response(correlation_id, "{\"chimeId\":\"" + json_escape(chime_id) + "\"}");
  }
  if (method == "removeChime") {
    const std::string chime_id = json_get_string(line, "chimeId");
    if (chime_id.empty()) {
      return error_response(correlation_id, "PAGING_VALIDATION_ERROR", "chimeId is required");
    }
    if (!context_.repository->remove_chime(chime_id)) {
      return error_response(correlation_id, "PAGING_CHIME_REMOVE_FAILED", "Failed to remove chime");
    }
    return ok_response(correlation_id, "{\"chimeId\":\"" + json_escape(chime_id) + "\"}");
  }
  if (method == "reloadVoice") {
    const std::string voice_id = json_get_string(line, "voiceId");
    const bool ok = context_.on_reload_voice ? context_.on_reload_voice(voice_id) : false;
    if (!ok) {
      return error_response(correlation_id, "PAGING_RELOAD_FAILED", "Failed to reload voice");
    }
    return ok_response(correlation_id, "{\"reloaded\":true}");
  }
  if (method == "previewVoice") {
    const std::string text = json_get_string(line, "text");
    const std::string voice_id = json_get_string(line, "voiceId");
    const bool ok = context_.on_preview_voice ? context_.on_preview_voice(text, voice_id) : false;
    if (!ok) {
      return error_response(correlation_id, "PAGING_PREVIEW_FAILED", "Failed to preview voice");
    }
    return ok_response(correlation_id, "{\"played\":true}");
  }
  if (method == "getApiKey") {
    const PagingApiKeyMetadata metadata = context_.repository->get_api_key();
    std::ostringstream data;
    data << "{\"configured\":" << (metadata.configured ? "true" : "false") << ",\"prefix\":";
    if (metadata.prefix.empty()) {
      data << "null";
    } else {
      data << "\"" << json_escape(metadata.prefix) << "\"";
    }
    data << ",\"hash\":";
    if (metadata.hash.empty()) {
      data << "null";
    } else {
      data << "\"" << json_escape(metadata.hash) << "\"";
    }
    data << "}";
    return ok_response(correlation_id, data.str());
  }
  if (method == "setApiKey") {
    const std::string key_hash = json_get_string(line, "apiKeyHash");
    const std::string key_prefix = json_get_string(line, "apiKeyPrefix");
    if (key_hash.empty() || key_prefix.empty()) {
      return error_response(correlation_id, "PAGING_VALIDATION_ERROR",
                            "apiKeyHash and apiKeyPrefix are required");
    }
    if (!context_.repository->set_api_key(key_hash, key_prefix)) {
      return error_response(correlation_id, "PAGING_API_KEY_UPDATE_FAILED", "Failed to store API key");
    }
    return ok_response(correlation_id, "{\"configured\":true,\"prefix\":\"" + json_escape(key_prefix) + "\"}");
  }
  if (method == "clearApiKey") {
    if (!context_.repository->clear_api_key()) {
      return error_response(correlation_id, "PAGING_API_KEY_CLEAR_FAILED", "Failed to clear API key");
    }
    return ok_response(correlation_id, "{\"configured\":false,\"prefix\":null}");
  }
  return error_response(correlation_id, "PAGING_UNKNOWN_METHOD", "Unknown method: " + method);
}

}  // namespace homepi::audio_paging
