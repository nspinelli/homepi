#pragma once

#include <string>

namespace homepi::audio_orchestrator {

/** Socket paths used for legacy direct RPC during broker migration. */
struct SocketPaths {
  std::string pcm_router;
  std::string hifi_serial;
  std::string nqptp_host = "127.0.0.1";
  int nqptp_port = 9000;
};

/**
 * Direct Unix-socket and UDP helpers for PCM router and Hi-Fi serial services.
 */
class ServiceSocketClient {
 public:
  /**
   * Creates a client for the given socket paths.
   * @param paths PCM, Hi-Fi, and NQPTP endpoints.
   */
  explicit ServiceSocketClient(SocketPaths paths);

  /**
   * Sends a PCM router routing command and returns the first response line.
   * @param method Routing method such as route_start or route_end.
   * @param zone_id Target zone id.
   * @returns Response line or empty on failure.
   */
  std::string pcm_route(const std::string& method, int zone_id) const;

  /**
   * Extracts ownerZoneId from a PCM snapshot response envelope.
   * @param response PCM router response line.
   * @returns Owner zone id or 0 when unavailable.
   */
  int pcm_owner_from_response(const std::string& response) const;

  /**
   * Sends a raw Hi-Fi controller command synchronously.
   * @param command Controller command such as *Z3POWER1.
   */
  void send_hifi_command(const std::string& command) const;

  /**
   * Sends a raw Hi-Fi controller command without blocking the caller.
   * @param command Controller command such as *Z3POWER1.
   */
  void send_hifi_command_async(const std::string& command) const;

  /**
   * Dispatches a typed Hi-Fi command without blocking the caller.
   * @param event Typed command event name.
   * @param payload_json Inner JSON fields without braces, e.g. "zoneNumber":3,"power":true.
   */
  void execute_hifi_command_async(const std::string& event,
                                  const std::string& payload_json) const;

  /** Sends the NQPTP play-begin control message. */
  void nqptp_play_begin() const;

 private:
  SocketPaths paths_;
};

}  // namespace homepi::audio_orchestrator
