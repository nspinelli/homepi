#include "homepi/pcm-router/frame-ring.hpp"

#include <algorithm>
#include <cstring>

namespace homepi::pcm_router {

bool FrameRing::init(size_t capacity_frames, size_t bytes_per_frame) {
  std::lock_guard lock(mutex_);
  capacity_frames_ = capacity_frames;
  bytes_per_frame_ = bytes_per_frame;
  buffer_.assign(capacity_frames * bytes_per_frame, 0);
  read_index_ = 0;
  write_index_ = 0;
  used_frames_ = 0;
  return capacity_frames_ > 0 && bytes_per_frame_ > 0;
}

void FrameRing::clear() {
  std::lock_guard lock(mutex_);
  read_index_ = 0;
  write_index_ = 0;
  used_frames_ = 0;
}

size_t FrameRing::write(const void* data, size_t frame_count) {
  std::lock_guard lock(mutex_);
  if (capacity_frames_ == 0 || bytes_per_frame_ == 0) {
    return 0;
  }
  for (size_t i = 0; i < frame_count; ++i) {
    if (used_frames_ >= capacity_frames_) {
      read_index_ = (read_index_ + 1) % capacity_frames_;
      used_frames_ = capacity_frames_ - 1;
    }
    std::memcpy(buffer_.data() + write_index_ * bytes_per_frame_,
                static_cast<const uint8_t*>(data) + i * bytes_per_frame_, bytes_per_frame_);
    write_index_ = (write_index_ + 1) % capacity_frames_;
    ++used_frames_;
  }
  return frame_count;
}

size_t FrameRing::read(void* data, size_t frame_count) {
  std::lock_guard lock(mutex_);
  const size_t to_read = std::min(frame_count, used_frames_);
  for (size_t i = 0; i < to_read; ++i) {
    std::memcpy(static_cast<uint8_t*>(data) + i * bytes_per_frame_,
                buffer_.data() + read_index_ * bytes_per_frame_, bytes_per_frame_);
    read_index_ = (read_index_ + 1) % capacity_frames_;
  }
  used_frames_ -= to_read;
  return to_read;
}

size_t FrameRing::available_frames() const {
  std::lock_guard lock(mutex_);
  return used_frames_;
}

}  // namespace homepi::pcm_router
