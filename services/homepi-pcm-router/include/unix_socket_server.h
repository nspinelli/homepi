#pragma once

#include <stdbool.h>

/**
 * Starts Unix domain socket server for NDJSON event subscribers.
 * @param socket_path Socket path.
 * @param snapshot_json_fn Returns snapshot payload JSON object.
 * @return True on success.
 */
bool unix_socket_server_start(const char* socket_path,
                              const char* (*snapshot_json_fn)(void));

/** Stops the socket server. */
void unix_socket_server_stop(void);

/**
 * Broadcasts a line to all subscribers.
 * @param line NDJSON line without trailing newline.
 */
void unix_socket_server_broadcast(const char* line);
