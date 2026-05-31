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
      return "DEBUG";
    case LOG_LEVEL_INFO:
      return "INFO";
    case LOG_LEVEL_WARN:
      return "WARN";
    case LOG_LEVEL_ERROR:
      return "ERROR";
    default:
      return "INFO";
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
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &tm);

  const char* module_str = module ? module : "";
  const char* event_str = event ? event : "";
  const char* message_str = message ? message : "";

  fprintf(stderr,
          "{\"ts\":\"%s\",\"service\":\"homepi-pcm-router\","
          "\"module\":\"%s\",\"level\":\"%s\",\"event\":\"%s\","
          "\"correlationId\":\"%s\",\"message\":\"%s\",\"data\":{}}\n",
          timestamp, module_str, level_name(level), event_str, event_str, message_str);
}
