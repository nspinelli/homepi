#include "homepi/audio-paging/events-subscriber.hpp"

#include "homepi/audio-paging/json-utils.hpp"
#include "homepi/events/events-client.hpp"

namespace homepi::audio_paging {

EventsSubscriber::EventsSubscriber(std::string events_socket, std::string source,
                                   EventsSubscriberCallbacks callbacks)
    : events_socket_(std::move(events_socket)),
      source_(std::move(source)),
      callbacks_(std::move(callbacks)) {}

void EventsSubscriber::start() {
  client_ = std::make_unique<homepi::events::EventsClient>(events_socket_, source_);
  client_->start(
      {"audio.paging.command", "audio.paging.command.*", "modules.audio.system"},
      {"audio.paging", "audio.paging.*"},
      [this](const std::string& line) { handle_event_line(line); });
}

void EventsSubscriber::stop() {
  if (client_ != nullptr) {
    client_->stop();
    client_.reset();
  }
}

void EventsSubscriber::handle_event_line(const std::string& line) const {
  const std::string topic = parse_topic_name(line);
  const std::string event = parse_event_name(line);
  if (topic == "modules.audio.system" && event == "page_state_changed") {
    if (callbacks_.on_page_state_changed) {
      callbacks_.on_page_state_changed(line);
    }
    return;
  }
  if (topic != "audio.paging.command" && topic != "audio.paging.command.speak" &&
      topic != "audio.paging.command.chime" && topic != "audio.paging.command.preview_voice" &&
      topic != "audio.paging.command.reload_voice" &&
      topic != "audio.paging.command.preview_chime" &&
      topic != "audio.paging.command.preview_page") {
    return;
  }
  if (event == "speak" || event == "audio.paging.command.speak") {
    if (callbacks_.on_speak) {
      callbacks_.on_speak(line);
    }
  } else if (event == "chime" || event == "audio.paging.command.chime") {
    if (callbacks_.on_chime) {
      callbacks_.on_chime(line);
    }
  } else if (event == "preview_chime" || event == "audio.paging.command.preview_chime") {
    if (callbacks_.on_preview_chime) {
      callbacks_.on_preview_chime(line);
    }
  } else if (event == "preview_page" || event == "audio.paging.command.preview_page") {
    if (callbacks_.on_speak) {
      callbacks_.on_speak(line);
    }
  } else if (event == "preview_voice" || event == "audio.paging.command.preview_voice") {
    if (callbacks_.on_preview_voice) {
      callbacks_.on_preview_voice(line);
    }
  } else if (event == "reload_voice" || event == "audio.paging.command.reload_voice") {
    if (callbacks_.on_reload_voice) {
      callbacks_.on_reload_voice(line);
    }
  }
}

}  // namespace homepi::audio_paging
