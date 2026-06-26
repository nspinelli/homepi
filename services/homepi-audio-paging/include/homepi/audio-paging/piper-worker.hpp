#pragma once

#include <optional>
#include <string>

namespace homepi::audio_paging {

/** Piper synthesis output descriptor for temporary raw PCM files. */
struct PiperOutput {
  std::string raw_path;
  int sample_rate_hz = 22050;
};

/** Piper subprocess manager with warm/cold lifecycle and output_raw synthesis. */
class PiperWorker {
 public:
  /**
   * Creates a worker manager.
   * @param piper_binary Executable path for piper.
   * @param default_voice_model Default ONNX path.
   * @param default_voice_config Default voice JSON config path.
   */
  PiperWorker(std::string piper_binary, std::string default_voice_model,
              std::string default_voice_config);

  /** Warms Piper state for the selected voice. */
  bool warm(const std::string& voice_model_path, const std::string& voice_config_path);

  /** Unloads warm state and marks worker cold. */
  void cool_down();

  /** Returns true when worker has an active warmed voice selection. */
  bool is_warm() const;

  /** Returns currently loaded voice model path. */
  std::string loaded_voice_model() const;

  /**
   * Synthesizes speech into a temporary raw file using piper --output_raw.
   * @param text Input text to synthesize.
   * @param voice_model_path Voice ONNX path.
   * @param voice_config_path Voice config JSON path.
   * @param keep_debug_copy When true, preserves generated file.
   */
  std::optional<PiperOutput> synthesize(const std::string& text, const std::string& voice_model_path,
                                        const std::string& voice_config_path, bool keep_debug_copy);

 private:
  std::string piper_binary_;
  std::string default_voice_model_;
  std::string default_voice_config_;
  std::string loaded_voice_model_;
  std::string loaded_voice_config_;
  bool warm_ = false;
};

}  // namespace homepi::audio_paging
