#include "nqptp_refresh.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"

#define NQPTP_CONTROL_PORT 9000

typedef struct HandoffRefreshRequest {
  int zone_id;
  char client_ip[64];
  unsigned int delay_ms;
} HandoffRefreshRequest;

static bool send_nqptp_control_message(const char* message) {
  if (!message || message[0] == '\0') {
    return false;
  }

  const int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return false;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(NQPTP_CONTROL_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  const ssize_t sent =
      sendto(fd, message, strlen(message), 0, (struct sockaddr*)&addr, sizeof(addr));
  close(fd);
  return sent == (ssize_t)strlen(message);
}

static void* handoff_refresh_thread(void* arg) {
  HandoffRefreshRequest* request = (HandoffRefreshRequest*)arg;
  if (request->delay_ms > 0) {
    usleep((useconds_t)request->delay_ms * 1000U);
  }

  send_nqptp_control_message("/nqptp B");

  if (request->client_ip[0] != '\0') {
    char timing_message[128];
    snprintf(timing_message, sizeof(timing_message), "/nqptp T %s", request->client_ip);
    send_nqptp_control_message(timing_message);
  }

  char detail[128];
  snprintf(detail, sizeof(detail), "zone=%d ip=%s", request->zone_id,
           request->client_ip[0] != '\0' ? request->client_ip : "none");
  log_msg(LOG_LEVEL_INFO, "nqptp_refresh", "owner_handoff", detail);
  free(request);
  return NULL;
}

void nqptp_schedule_owner_handoff_refresh(int zone_id, const char* client_ip,
                                          unsigned int delay_ms) {
  HandoffRefreshRequest* request = (HandoffRefreshRequest*)calloc(1, sizeof(*request));
  if (!request) {
    return;
  }
  request->zone_id = zone_id;
  request->delay_ms = delay_ms;
  if (client_ip) {
    snprintf(request->client_ip, sizeof(request->client_ip), "%s", client_ip);
  }

  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&thread, &attr, handoff_refresh_thread, request) != 0) {
    free(request);
  }
  pthread_attr_destroy(&attr);
}
