#pragma once

/** Log levels for structured stderr logging. */
typedef enum LogLevel { LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR } LogLevel;

/**
 * Sets minimum log level from string.
 * @param level Level name.
 */
void log_set_level(const char* level);

/**
 * Emits a structured log line.
 * @param level Log level.
 * @param module Module name.
 * @param event Event name.
 * @param message Human message.
 */
void log_msg(LogLevel level, const char* module, const char* event, const char* message);
