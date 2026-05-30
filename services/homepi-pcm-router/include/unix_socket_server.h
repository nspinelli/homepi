#pragma once

#include <stdbool.h>

/**
 * Handles inbound control commands from Unix socket clients.
 * @param method Command method name (e.g. route_start).
 * @param zone_id Target zone id.
 * @param user Opaque user pointer from registration.
 */
typedef void (*UnixSocketCommandFn)(const char* method, int zone_id, void* user);

/**
 * Starts Unix domain socket server for NDJSON event subscribers.
 * @param socket_path Socket path.
 * @param snapshot_json_fn Returns snapshot payload JSON object.
 * @param command_fn Optional handler for route_start/route_end commands.
 * @param command_user Opaque pointer passed to command_fn.
 * @return True on success.
 */
bool unix_socket_server_start(const char* socket_path,
                              const char* (*snapshot_json_fn)(void),
                              UnixSocketCommandFn command_fn,
                              void* command_user);

/** Stops the socket server. */
void unix_socket_server_stop(void);

/**
 * Broadcasts a line to all subscribers.
 * @param line NDJSON line without trailing newline.
 */
void unix_socket_server_broadcast(const char* line);
