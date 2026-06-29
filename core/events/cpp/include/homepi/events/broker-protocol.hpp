#pragma once

#include <optional>
#include <string>
#include <vector>

namespace homepi::events {

/**
 * Returns true when the socket path targets the v2 homepi-broker service.
 * @param socket_path Unix socket path.
 * @returns True for canonical broker socket paths.
 */
bool is_v2_broker_socket(const std::string& socket_path);

/**
 * Builds a v2 broker subscribe command line for the given topics.
 * @param source Calling service name.
 * @param topics Topic patterns to subscribe to.
 * @returns NDJSON command without trailing newline.
 */
std::string build_broker_subscribe_line(const std::string& source,
                                        const std::vector<std::string>& topics);

/**
 * Converts a legacy event envelope line into a v2 broker publish command.
 * @param source Calling service name.
 * @param legacy_envelope_line Legacy event envelope JSON.
 * @returns NDJSON publish command without trailing newline.
 */
std::string build_broker_publish_line(const std::string& source,
                                      const std::string& legacy_envelope_line);

/**
 * Converts a v2 broker wire line into a legacy event envelope when applicable.
 * @param wire_line Raw NDJSON line from homepi-broker.
 * @returns Legacy envelope JSON or nullopt for non-event lines.
 */
std::optional<std::string> broker_wire_to_legacy_envelope(const std::string& wire_line);

}  // namespace homepi::events
