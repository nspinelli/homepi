#include "log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static LogLevel g_min_level = LOG_LEVEL_INFO;

static LogLevel parse_level(const char* level) {
  if (!level) {
    return LOG_LEVEL_INFO;
  }
  if (strcmp(level, "debug") == 0) {
    return LOG_LEVEL_DEBUG;
  }
  if (strcmp(level, "warn") == 0 || strcmp(level, "warning") == 0) {
    return LOG_LEVEL_WARN;
  }
  if (strcmp(level, "error") == 0) {
    return LOG_LEVEL_ERROR;
  }
  return LOG_LEVEL_INFO;
}

void log_set_level(const char* level) { g_min_level = parse_level(level); }

static const char* level_name(LogLevel level) {
  switch (level) {
    case LOG_LEVEL_DEBUG:
      return "debug";
    case LOG_LEVEL_INFO:
      return "info";
    case LOG_LEVEL_WARN:
      return "warn";
    case LOG_LEVEL_ERROR:
      return "error";
    default:
      return "info";
  }
}

void log_msg(LogLevel level, const char* module, const char* event, const char* message) {
  if (level < g_min_level) {
    return;
  }

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm;
  gmtime_r(&ts.tv_sec, &tm);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &tm);

  fprintf(stderr,
          "{\"timestamp\":\"%s.000Z\",\"level\":\"%s\",\"service\":\"homepi-pcm-router\","
          "\"module\":\"%s\",\"event\":\"%s\",\"message\":\"%s\"}\n",
          timestamp, level_name(level), module ? module : "", event ? event : "",
          message ? message : "");
}
