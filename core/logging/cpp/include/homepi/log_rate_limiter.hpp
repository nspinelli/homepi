#pragma once

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace homepi::logging {

/**
 * Sliding-window rate limiter for log events.
 */
class LogRateLimiter {
 public:
  LogRateLimiter(int max_per_window = 100,
                 std::chrono::milliseconds window = std::chrono::milliseconds(10000))
      : max_per_window_(max_per_window), window_(window) {}

  bool should_emit(const std::string& key) {
    const auto now = std::chrono::steady_clock::now();
    auto& timestamps = buckets_[key];
    timestamps.erase(
        std::remove_if(timestamps.begin(), timestamps.end(),
                      [&](const auto& t) { return now - t >= window_; }),
        timestamps.end());

    if (static_cast<int>(timestamps.size()) >= max_per_window_) {
      return false;
    }
    timestamps.push_back(now);
    return true;
  }

 private:
  int max_per_window_;
  std::chrono::milliseconds window_;
  std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>>
      buckets_;
};

}  // namespace homepi::logging
