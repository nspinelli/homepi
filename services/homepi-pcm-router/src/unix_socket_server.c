#include "unix_socket_server.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int g_server_fd = -1;
static pthread_t g_thread;
static bool g_stop = false;
static char g_socket_path[256];
static const char* (*g_snapshot_fn)(void) = NULL;
static int g_subscribers[32];
static size_t g_subscriber_count = 0;
static pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static void iso_timestamp(char* out, size_t out_len) {
  time_t now = time(NULL);
  struct tm tm;
  gmtime_r(&now, &tm);
  strftime(out, out_len, "%Y-%m-%dT%H:%M:%S.000Z", &tm);
}

static void send_snapshot(int client_fd) {
  if (!g_snapshot_fn) {
    return;
  }
  const char* payload = g_snapshot_fn();
  char timestamp[40];
  iso_timestamp(timestamp, sizeof(timestamp));
  char line[4096];
  snprintf(line, sizeof(line),
           "{\"version\":1,\"id\":\"evt-pcm-snapshot\",\"source\":\"homepi-pcm-router\","
           "\"topic\":\"modules.pcm.snapshot\",\"event\":\"pcm_router_snapshot\","
           "\"correlationId\":\"connect\",\"timestamp\":\"%s\",\"payload\":%s}",
           timestamp, payload ? payload : "{}");
  const char* frame = line;
  size_t len = strlen(frame);
  char buf[4200];
  memcpy(buf, frame, len);
  buf[len++] = '\n';
  (void)write(client_fd, buf, len);
}

void unix_socket_server_broadcast(const char* line) {
  if (!line) {
    return;
  }
  char frame[4200];
  const size_t len = snprintf(frame, sizeof(frame), "%s\n", line);
  pthread_mutex_lock(&g_clients_mutex);
  for (size_t i = 0; i < g_subscriber_count;) {
    const ssize_t written = write(g_subscribers[i], frame, len);
    if (written < 0) {
      close(g_subscribers[i]);
      g_subscribers[i] = g_subscribers[g_subscriber_count - 1];
      g_subscriber_count--;
    } else {
      i++;
    }
  }
  pthread_mutex_unlock(&g_clients_mutex);
}

static void* client_thread(void* arg) {
  const int client_fd = (int)(intptr_t)arg;
  char buffer[4096];
  size_t buflen = 0;
  bool subscribed = false;

  while (!g_stop) {
    const ssize_t n = read(client_fd, buffer + buflen, sizeof(buffer) - buflen - 1);
    if (n <= 0) {
      break;
    }
    buflen += (size_t)n;
    buffer[buflen] = '\0';

    char* line = strchr(buffer, '\n');
    while (line) {
      *line = '\0';
      if (strstr(buffer, "\"subscribe\"") != NULL || strstr(buffer, "\"method\":\"subscribe\"") != NULL) {
        if (!subscribed) {
          subscribed = true;
          pthread_mutex_lock(&g_clients_mutex);
          if (g_subscriber_count < sizeof(g_subscribers) / sizeof(g_subscribers[0])) {
            g_subscribers[g_subscriber_count++] = client_fd;
          }
          pthread_mutex_unlock(&g_clients_mutex);
          send_snapshot(client_fd);
        }
      }
      buflen = 0;
      line = NULL;
    }
  }

  pthread_mutex_lock(&g_clients_mutex);
  for (size_t i = 0; i < g_subscriber_count; ++i) {
    if (g_subscribers[i] == client_fd) {
      g_subscribers[i] = g_subscribers[g_subscriber_count - 1];
      g_subscriber_count--;
      break;
    }
  }
  pthread_mutex_unlock(&g_clients_mutex);
  close(client_fd);
  return NULL;
}

static void* listen_thread(void* arg) {
  (void)arg;
  while (!g_stop) {
    const int client = accept(g_server_fd, NULL, NULL);
    if (client < 0) {
      if (g_stop) {
        break;
      }
      continue;
    }
    pthread_t t;
    pthread_create(&t, NULL, client_thread, (void*)(intptr_t)client);
    pthread_detach(t);
  }
  return NULL;
}

bool unix_socket_server_start(const char* socket_path, const char* (*snapshot_json_fn)(void)) {
  g_snapshot_fn = snapshot_json_fn;
  snprintf(g_socket_path, sizeof(g_socket_path), "%s", socket_path);
  unlink(socket_path);

  g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (g_server_fd < 0) {
    return false;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  if (bind(g_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(g_server_fd);
    g_server_fd = -1;
    return false;
  }
  if (listen(g_server_fd, 16) < 0) {
    close(g_server_fd);
    g_server_fd = -1;
    return false;
  }

  g_stop = false;
  pthread_create(&g_thread, NULL, listen_thread, NULL);
  return true;
}

void unix_socket_server_stop(void) {
  g_stop = true;
  if (g_server_fd >= 0) {
    shutdown(g_server_fd, SHUT_RDWR);
    close(g_server_fd);
    g_server_fd = -1;
  }
  pthread_join(g_thread, NULL);
  unlink(g_socket_path);
}
