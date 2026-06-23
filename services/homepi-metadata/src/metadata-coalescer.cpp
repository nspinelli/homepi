#include "homepi/metadata/metadata-coalescer.hpp"

namespace homepi::metadata {

MetadataCoalescer::MetadataCoalescer(int debounce_ms, FlushFn on_flush)
    : debounce_ms_(debounce_ms), on_flush_(std::move(on_flush)) {}

void MetadataCoalescer::set_awaiting_bundle_end(bool awaiting) {
  awaiting_bundle_end_.store(awaiting);
}

bool MetadataCoalescer::awaiting_bundle_end() const { return awaiting_bundle_end_.load(); }

void MetadataCoalescer::schedule_flush(const std::string& reason, bool immediate) {
  if (awaiting_bundle_end_.load() && !immediate) {
    return;
  }
  std::lock_guard lock(mutex_);
  pending_reason_ = reason;
  flush_scheduled_ = true;
  const int delay_ms = immediate ? 0 : debounce_ms_;
  flush_at_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
}

void MetadataCoalescer::cancel_pending() {
  std::lock_guard lock(mutex_);
  flush_scheduled_ = false;
  pending_reason_.clear();
}

void MetadataCoalescer::tick() {
  if (awaiting_bundle_end_.load()) {
    return;
  }

  std::string reason;
  {
    std::lock_guard lock(mutex_);
    if (!flush_scheduled_) {
      return;
    }
    if (std::chrono::steady_clock::now() < flush_at_) {
      return;
    }
    reason = std::move(pending_reason_);
    flush_scheduled_ = false;
  }

  if (on_flush_) {
    on_flush_(reason);
  }
}

}  // namespace homepi::metadata
