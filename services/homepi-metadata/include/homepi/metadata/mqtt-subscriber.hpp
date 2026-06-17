#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct mosquitto;
struct mosquitto_message;

namespace homepi::metadata {

/** Callbacks for MQTT metadata events on the PCM owner zone. */
struct MqttSubscriberCallbacks {
  std::function<void(int zone_id, const std::string& field, const std::string& value)> on_field;
  std::function<void(int zone_id, int position_ms, int duration_ms, bool playing)> on_progress;
  std::function<void(int zone_id, bool playing)> on_playback_state;
  std::function<void(int zone_id, const std::vector<std::uint8_t>&)> on_cover_art;
  std::function<void(int zone_id)> on_metadata_bundle_start;
  std::function<void(int zone_id)> on_session_cleared;
};

/**
 * Subscribes to Shairport Sync parsed (and raw progress) MQTT topics for the owner zone.
 */
class MqttSubscriber {
 public:
  MqttSubscriber();
  ~MqttSubscriber();

  MqttSubscriber(const MqttSubscriber&) = delete;
  MqttSubscriber& operator=(const MqttSubscriber&) = delete;

  /**
   * Connects to the MQTT broker and prepares for zone subscriptions.
   * @param host Broker hostname.
   * @param port Broker port.
   * @param topic_prefix Topic prefix before zone id (e.g. shairport/zone).
   * @param callbacks Metadata event callbacks.
   * @returns True when the broker connection succeeds.
   */
  bool start(const std::string& host, int port, const std::string& topic_prefix,
             MqttSubscriberCallbacks callbacks);

  /** Disconnects and stops the mosquitto network loop. */
  void stop();

  /**
   * Resubscribes to MQTT topics for the active owner zone.
   * @param owner_zone_id PCM owner zone id, or 0 to unsubscribe.
   */
  void set_owner_zone(int owner_zone_id);

 private:
  void on_message(const ::mosquitto_message* message);
  void handle_topic_suffix(int zone_id, const std::string& suffix,
                           const std::vector<std::uint8_t>& payload);
  bool subscribe_zone(int zone_id);
  void unsubscribe_zone(int zone_id);
  std::string topic_for(int zone_id, const char* suffix) const;

  static void message_callback(struct ::mosquitto* mosq, void* userdata,
                               const struct ::mosquitto_message* message);

  std::string host_;
  int port_ = 1883;
  std::string topic_prefix_ = "shairport/zone";
  MqttSubscriberCallbacks callbacks_;
  struct ::mosquitto* mosq_ = nullptr;
  std::atomic<int> owner_zone_id_{0};
  std::atomic<int> subscribed_zone_id_{0};
  std::mutex mutex_;
  int sample_rate_hz_ = 44100;
  long long progress_start_rtp_ = 0;
};

}  // namespace homepi::metadata
