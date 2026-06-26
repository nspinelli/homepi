#pragma once

#include <atomic>
#include <string>

namespace homepi::events {
class EventEmitter;
}

namespace homepi::audio_paging {

/** Publishes page_start/page_end commands and tracks hifi page state updates. */
class HifiPagingController {
 public:
  /**
   * Creates controller with event emitter sink.
   * @param emitter - Broker event emitter.
   * @param hifi_serial_socket - Unix socket path for hifi-serial snapshot sync.
   */
  HifiPagingController(homepi::events::EventEmitter* emitter, std::string hifi_serial_socket);

  /** Sends page_start command to modules.hifi.command. */
  void request_page_on(const std::string& correlation_id);

  /** Sends page_end command to modules.hifi.command. */
  void request_page_off(const std::string& correlation_id);

  /** Updates page state from modules.audio.system page_state_changed events. */
  void on_page_state_changed(int page_state);

  /** Waits for PAGE1 active state with timeout in ms. */
  bool wait_for_page_on(int timeout_ms);

  /** Waits for PAGE0 idle state with timeout in ms. */
  bool wait_for_page_off(int timeout_ms);

  /**
   * Waits for page-on confirmation, zone power-up, and analog settle time.
   * @param page_on_timeout_ms - Timeout for PAGE1 confirmation.
   * @param settle_ms - Extra delay after zones report powered before playback.
   */
  bool wait_for_page_playback_ready(int page_on_timeout_ms, int settle_ms);

  /** Returns true when external hardware page mode is active (#PAGE3). */
  bool external_page_active() const;

 private:
  /** Refreshes page state from hifi-serial snapshot when available. */
  void sync_page_state_from_hifi();

  /** Returns true when at least one zone reports power on in the snapshot. */
  bool any_zone_powered_in_snapshot();

  homepi::events::EventEmitter* emitter_ = nullptr;
  std::string hifi_serial_socket_;
  std::atomic<int> page_state_{0};
};

}  // namespace homepi::audio_paging
