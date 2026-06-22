#pragma once

#include <string>

namespace homepi::transport {

/**
 * Creates and binds a listening Unix domain stream socket.
 * @param path Absolute socket path.
 * @param backlog Listen backlog.
 * @returns File descriptor or -1 on failure.
 */
int create_listening_unix_stream_socket(const std::string& path, int backlog = 16);

/**
 * Connects a client to a Unix domain stream socket.
 * @param path Absolute socket path.
 * @returns File descriptor or -1 on failure.
 */
int connect_unix_stream_socket(const std::string& path);

/**
 * Sets non-blocking mode on a file descriptor.
 * @param fd File descriptor.
 * @returns True on success.
 */
bool set_nonblocking(int fd);

/**
 * Applies owner/group/mode to a socket path after bind.
 * @param path Socket path.
 * @param mode Permission bits (e.g. 0660).
 * @param group Group name (e.g. homepi).
 * @returns True on success.
 */
bool apply_socket_permissions(const std::string& path, unsigned int mode,
                              const std::string& group_name);

/**
 * Removes a socket path if present.
 * @param path Socket path.
 */
void remove_socket_path(const std::string& path);

}  // namespace homepi::transport
