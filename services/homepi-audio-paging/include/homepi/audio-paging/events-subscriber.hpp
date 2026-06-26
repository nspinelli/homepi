#pragma once

#include <functional>
#include <memory>
#include <string>

namespace homepi::events {
class EventsClient;
}

namespace homepi::audio_paging {

/** Broker command callbacks consumed by orchestrator and hifi controller. */
struct EventsSubscriberCallbacks {
  std::function<void(const std::string& line)> on_speak;
  std::function<void(const std::string& line)> on_chime;
  std::function<void(const std::string& line)> on_preview_chime;
  std::function<void(const std::string& line)> on_preview_voice;
  std::function<void(const std::string& line)> on_reload_voice;
  std::function<void(const std::string& line)> on_page_state_changed;
};

/** Subscribes to audio.paging.command.* and page state events on the broker. */
class EventsSubscriber {
 public:
  /** Creates subscriber with socket path, source name, and callbacks. */
  EventsSubscriber(std::string events_socket, std::string source, EventsSubscriberCallbacks callbacks);

  /** Starts broker subscription loop. */
  void start();

  /** Stops broker subscription loop. */
  void stop();

 private:
  void handle_event_line(const std::string& line) const;

  std::string events_socket_;
  std::string source_;
  EventsSubscriberCallbacks callbacks_;
  std::unique_ptr<homepi::events::EventsClient> client_;
};

}  // namespace homepi::audio_paging
