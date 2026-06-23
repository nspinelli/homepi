#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>

namespace homepi::metadata {

/**
 * Debounces metadata snapshot emissions while a Shairport bundle is open.
 */
class MetadataCoalescer {
 public:
  /** Invoked when a coalesced snapshot flush is due. */
  using FlushFn = std::function<void(const std::string& reason)>;

  /**
   * Creates a coalescer with the given debounce interval.
   * @param debounce_ms Milliseconds to wait before flushing scheduled snapshots.
   * @param on_flush Callback invoked with the flush reason.
   */
  MetadataCoalescer(int debounce_ms, FlushFn on_flush);

  /**
   * Blocks scheduled flushes while a metadata bundle is incomplete.
   * @param awaiting True between mdst and mden.
   */
  void set_awaiting_bundle_end(bool awaiting);

  /**
   * Returns whether a bundle is still open.
   * @returns True when mdst arrived without a matching mden.
   */
  bool awaiting_bundle_end() const;

  /**
   * Schedules a snapshot flush after debounce (or immediately).
   * @param reason Diagnostic reason for the flush.
   * @param immediate When true, flush on the next tick without debounce.
   */
  void schedule_flush(const std::string& reason, bool immediate = false);

  /** Cancels any pending scheduled flush. */
  void cancel_pending();

  /**
   * Advances timers and invokes the flush callback when due.
   * Call periodically from the service main loop.
   */
  void tick();

 private:
  int debounce_ms_;
  FlushFn on_flush_;
  mutable std::mutex mutex_;
  std::atomic<bool> awaiting_bundle_end_{false};
  bool flush_scheduled_ = false;
  std::chrono::steady_clock::time_point flush_at_{};
  std::string pending_reason_;
};

}  // namespace homepi::metadata
