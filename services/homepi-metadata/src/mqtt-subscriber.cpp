#include "homepi/metadata/mqtt-subscriber.hpp"

#include <mosquitto.h>

#include <cstring>
#include <string_view>

#include "homepi/metadata/progress-parser.hpp"

namespace homepi::metadata {

namespace {

constexpr const char* kEmptyPayload = "--";

bool is_empty_payload(const std::vector<std::uint8_t>& payload) {
  if (payload.empty()) {
    return true;
  }
  const std::string_view text(reinterpret_cast<const char*>(payload.data()), payload.size());
  return text == kEmptyPayload;
}

std::string payload_to_string(const std::vector<std::uint8_t>& payload) {
  return std::string(reinterpret_cast<const char*>(payload.data()), payload.size());
}

std::string suffix_from_topic(const std::string& topic, const std::string& prefix, int zone_id) {
  const std::string base = prefix + "/" + std::to_string(zone_id) + "/";
  if (topic.rfind(base, 0) != 0) {
    return {};
  }
  return topic.substr(base.size());
}

}  // namespace

MqttSubscriber::MqttSubscriber() = default;

MqttSubscriber::~MqttSubscriber() { stop(); }

bool MqttSubscriber::start(const std::string& host, int port, const std::string& topic_prefix,
                           MqttSubscriberCallbacks callbacks) {
  stop();
  host_ = host;
  port_ = port;
  topic_prefix_ = topic_prefix;
  callbacks_ = std::move(callbacks);

  mosquitto_lib_init();
  mosq_ = mosquitto_new("homepi-metadata", true, this);
  if (mosq_ == nullptr) {
    return false;
  }

  mosquitto_message_callback_set(mosq_, message_callback);

  if (mosquitto_connect(mosq_, host_.c_str(), port_, 60) != MOSQ_ERR_SUCCESS) {
    mosquitto_destroy(mosq_);
    mosq_ = nullptr;
    mosquitto_lib_cleanup();
    return false;
  }

  if (mosquitto_loop_start(mosq_) != MOSQ_ERR_SUCCESS) {
    mosquitto_disconnect(mosq_);
    mosquitto_destroy(mosq_);
    mosq_ = nullptr;
    mosquitto_lib_cleanup();
    return false;
  }

  return true;
}

void MqttSubscriber::stop() {
  const int zone = subscribed_zone_id_.exchange(0);
  if (mosq_ != nullptr && zone > 0) {
    unsubscribe_zone(zone);
  }

  if (mosq_ != nullptr) {
    mosquitto_loop_stop(mosq_, true);
    mosquitto_disconnect(mosq_);
    mosquitto_destroy(mosq_);
    mosq_ = nullptr;
    mosquitto_lib_cleanup();
  }

  owner_zone_id_.store(0);
  subscribed_zone_id_.store(0);
  progress_start_rtp_ = 0;
}

void MqttSubscriber::set_owner_zone(int owner_zone_id) {
  owner_zone_id_.store(owner_zone_id);

  const int previous = subscribed_zone_id_.exchange(0);
  if (mosq_ == nullptr) {
    return;
  }

  if (previous > 0) {
    unsubscribe_zone(previous);
  }

  progress_start_rtp_ = 0;
  if (owner_zone_id > 0) {
    if (subscribe_zone(owner_zone_id)) {
      subscribed_zone_id_.store(owner_zone_id);
    }
  }
}

std::string MqttSubscriber::topic_for(int zone_id, const char* suffix) const {
  return topic_prefix_ + "/" + std::to_string(zone_id) + "/" + suffix;
}

bool MqttSubscriber::subscribe_zone(int zone_id) {
  if (mosq_ == nullptr) {
    return false;
  }

  static constexpr const char* kTopics[] = {
      "title",
      "artist",
      "album",
      "client_name",
      "client_model",
      "playing",
      "cover",
      "frame_position_and_time",
      "first_frame_position_and_time",
      "active_start",
      "active_end",
      "play_end",
      "ssnc/prgr",
      "ssnc/snam",
      "core/astm",
  };

  for (const char* suffix : kTopics) {
    const std::string topic = topic_for(zone_id, suffix);
    if (mosquitto_subscribe(mosq_, nullptr, topic.c_str(), 0) != MOSQ_ERR_SUCCESS) {
      return false;
    }
  }
  return true;
}

void MqttSubscriber::unsubscribe_zone(int zone_id) {
  if (mosq_ == nullptr) {
    return;
  }

  static constexpr const char* kTopics[] = {
      "title",
      "artist",
      "album",
      "client_name",
      "client_model",
      "playing",
      "cover",
      "frame_position_and_time",
      "first_frame_position_and_time",
      "active_start",
      "active_end",
      "play_end",
      "ssnc/prgr",
      "ssnc/snam",
      "core/astm",
  };

  for (const char* suffix : kTopics) {
    const std::string topic = topic_for(zone_id, suffix);
    mosquitto_unsubscribe(mosq_, nullptr, topic.c_str());
  }
}

void MqttSubscriber::message_callback(struct ::mosquitto* mosq, void* userdata,
                                      const struct ::mosquitto_message* message) {
  (void)mosq;
  auto* self = static_cast<MqttSubscriber*>(userdata);
  if (self != nullptr && message != nullptr) {
    self->on_message(message);
  }
}

void MqttSubscriber::on_message(const ::mosquitto_message* message) {
  if (message == nullptr || message->topic == nullptr) {
    return;
  }

  const int zone_id = subscribed_zone_id_.load();
  if (zone_id <= 0) {
    return;
  }

  const std::string topic(message->topic);
  const std::string suffix = suffix_from_topic(topic, topic_prefix_, zone_id);
  if (suffix.empty()) {
    return;
  }

  std::vector<std::uint8_t> payload;
  if (message->payload != nullptr && message->payloadlen > 0) {
    const auto* bytes = static_cast<const std::uint8_t*>(message->payload);
    payload.assign(bytes, bytes + message->payloadlen);
  }

  handle_topic_suffix(zone_id, suffix, payload);
}

void MqttSubscriber::handle_topic_suffix(int zone_id, const std::string& suffix,
                                           const std::vector<std::uint8_t>& payload) {
  if (suffix == "cover") {
    if (!payload.empty() && !is_empty_payload(payload) && callbacks_.on_cover_art) {
      callbacks_.on_cover_art(zone_id, payload);
    }
    return;
  }

  if (is_empty_payload(payload)) {
    return;
  }

  if (suffix == "title" && callbacks_.on_field) {
    callbacks_.on_field(zone_id, "title", payload_to_string(payload));
    return;
  }
  if (suffix == "artist" && callbacks_.on_field) {
    callbacks_.on_field(zone_id, "artist", payload_to_string(payload));
    return;
  }
  if (suffix == "album" && callbacks_.on_field) {
    callbacks_.on_field(zone_id, "album", payload_to_string(payload));
    return;
  }
  if (suffix == "client_name" && callbacks_.on_field) {
    callbacks_.on_field(zone_id, "client_name", payload_to_string(payload));
    return;
  }
  if (suffix == "ssnc/snam" && callbacks_.on_field) {
    callbacks_.on_field(zone_id, "client_name", payload_to_string(payload));
    return;
  }
  if (suffix == "client_model" && callbacks_.on_field) {
    callbacks_.on_field(zone_id, "client_model", payload_to_string(payload));
    return;
  }

  if (suffix == "playing" && callbacks_.on_playback_state) {
    const std::string value = payload_to_string(payload);
    callbacks_.on_playback_state(zone_id, value == "1");
    return;
  }

  if (suffix == "active_start") {
    progress_start_rtp_ = 0;
    if (callbacks_.on_metadata_bundle_start) {
      callbacks_.on_metadata_bundle_start(zone_id);
    }
    return;
  }

  if (suffix == "active_end" || suffix == "play_end") {
    progress_start_rtp_ = 0;
    if (callbacks_.on_session_cleared) {
      callbacks_.on_session_cleared(zone_id);
    }
    return;
  }

  if (suffix == "first_frame_position_and_time") {
    progress_start_rtp_ = parse_progress_start_rtp(payload_to_string(payload));
    return;
  }

  if (suffix == "frame_position_and_time" && callbacks_.on_progress) {
    const std::string text = payload_to_string(payload);
    if (progress_start_rtp_ == 0) {
      progress_start_rtp_ = parse_progress_start_rtp(text);
    }
    const auto update = parse_phbt_progress(text, sample_rate_hz_, progress_start_rtp_);
    if (update.has_position) {
      callbacks_.on_progress(zone_id, update.position_ms, -1, update.playing);
    }
    return;
  }

  if (suffix == "ssnc/prgr" && callbacks_.on_progress) {
    const std::string progress = payload_to_string(payload);
    progress_start_rtp_ = parse_progress_start_rtp(progress);
    const auto update = parse_prgr_progress(progress, sample_rate_hz_);
    if (update.has_position || update.has_duration) {
      callbacks_.on_progress(
          zone_id,
          update.has_position ? update.position_ms : -1,
          update.has_duration ? update.duration_ms : -1,
          update.playing);
    }
    return;
  }

  if (suffix == "core/astm" && callbacks_.on_progress) {
    const auto update = parse_astm_duration(payload);
    if (update.has_duration) {
      callbacks_.on_progress(zone_id, -1, update.duration_ms, update.playing);
    }
  }
}

}  // namespace homepi::metadata
