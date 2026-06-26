#include "homepi/audio-paging/resource-manager.hpp"

namespace homepi::audio_paging {

ResourceManager::ResourceManager() = default;

void ResourceManager::apply_config(const PagingConfig& config) {
  if (!config.enabled) {
    state_ = ResourceState::Disabled;
    return;
  }
  if (state_ == ResourceState::Disabled) {
    state_ = config.idle_policy == PagingIdlePolicy::AlwaysWarm ? ResourceState::Warm
                                                                : ResourceState::Cold;
  }
}

void ResourceManager::on_job_started() { state_ = ResourceState::Active; }

bool ResourceManager::on_job_finished(const PagingConfig& config) {
  last_idle_at_ = std::chrono::steady_clock::now();
  if (config.idle_policy == PagingIdlePolicy::AlwaysWarm) {
    state_ = ResourceState::Warm;
    return true;
  }
  state_ = ResourceState::Warm;
  return true;
}

bool ResourceManager::should_cool_down(const PagingConfig& config) const {
  if (state_ != ResourceState::Warm) {
    return false;
  }
  if (config.idle_policy == PagingIdlePolicy::AlwaysWarm) {
    return false;
  }
  const auto now = std::chrono::steady_clock::now();
  return now - last_idle_at_ >= std::chrono::milliseconds(config.idle_warm_timeout_ms);
}

void ResourceManager::force_cold() { state_ = ResourceState::Cold; }

void ResourceManager::mark_warm() { state_ = ResourceState::Warm; }

ResourceState ResourceManager::state() const { return state_; }

}  // namespace homepi::audio_paging
