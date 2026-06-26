#include "homepi/audio-paging/piper-worker.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace homepi::audio_paging {

namespace {

std::string temp_raw_path() {
  const auto now = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  return "/tmp/homepi-audio-paging-" + now + ".raw";
}

std::string temp_text_path() {
  const auto now = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  return "/tmp/homepi-audio-paging-text-" + now + ".txt";
}

std::string shell_quote(const std::string& value) {
  std::string quoted = "'";
  for (const char ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted += ch;
    }
  }
  quoted += "'";
  return quoted;
}

}  // namespace

PiperWorker::PiperWorker(std::string piper_binary, std::string default_voice_model,
                         std::string default_voice_config)
    : piper_binary_(std::move(piper_binary)),
      default_voice_model_(std::move(default_voice_model)),
      default_voice_config_(std::move(default_voice_config)),
      loaded_voice_model_(default_voice_model_),
      loaded_voice_config_(default_voice_config_) {}

bool PiperWorker::warm(const std::string& voice_model_path, const std::string& voice_config_path) {
  loaded_voice_model_ = voice_model_path.empty() ? default_voice_model_ : voice_model_path;
  loaded_voice_config_ = voice_config_path.empty() ? default_voice_config_ : voice_config_path;
  warm_ = fs::exists(loaded_voice_model_) && fs::exists(loaded_voice_config_) &&
          fs::exists(piper_binary_);
  return warm_;
}

void PiperWorker::cool_down() { warm_ = false; }

bool PiperWorker::is_warm() const { return warm_; }

std::string PiperWorker::loaded_voice_model() const { return loaded_voice_model_; }

std::optional<PiperOutput> PiperWorker::synthesize(const std::string& text,
                                                   const std::string& voice_model_path,
                                                   const std::string& voice_config_path,
                                                   bool keep_debug_copy) {
  const std::string model = voice_model_path.empty() ? loaded_voice_model_ : voice_model_path;
  const std::string config = voice_config_path.empty() ? loaded_voice_config_ : voice_config_path;
  if (!warm(model, config)) {
    return std::nullopt;
  }
  const std::string output_path = temp_raw_path();
  const std::string text_path = temp_text_path();
  {
    std::ofstream text_out(text_path);
    if (!text_out.is_open()) {
      return std::nullopt;
    }
    text_out << text;
  }

  const std::string command = "\"" + piper_binary_ + "\" --model " + shell_quote(model) +
                              " --config " + shell_quote(config) + " --output_raw < " +
                              shell_quote(text_path) + " > " + shell_quote(output_path);
  const int rc = std::system(command.c_str());
  if (fs::exists(text_path)) {
    fs::remove(text_path);
  }
  if (rc != 0) {
    return std::nullopt;
  }
  if (!fs::exists(output_path)) {
    return std::nullopt;
  }
  PiperOutput output;
  output.raw_path = output_path;
  output.sample_rate_hz = 22050;
  if (!keep_debug_copy) {
    // The caller consumes this file immediately and may remove it.
  }
  return output;
}

}  // namespace homepi::audio_paging
