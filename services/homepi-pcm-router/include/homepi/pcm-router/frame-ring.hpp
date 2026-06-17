#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace homepi::pcm_router {

/** Thread-safe PCM frame ring for one zone. */
class FrameRing {
 public:
  /**
   * Initializes ring capacity.
   * @param capacity_frames Frame capacity.
   * @param bytes_per_frame Bytes per frame.
   * @return True on success.
   */
  bool init(size_t capacity_frames, size_t bytes_per_frame);

  void clear();
  size_t write(const void* data, size_t frame_count);
  size_t read(void* data, size_t frame_count);
  size_t available_frames() const;

 private:
  mutable std::mutex mutex_;
  std::vector<uint8_t> buffer_;
  size_t capacity_frames_ = 0;
  size_t bytes_per_frame_ = 0;
  size_t read_index_ = 0;
  size_t write_index_ = 0;
  size_t used_frames_ = 0;
};

}  // namespace homepi::pcm_router
