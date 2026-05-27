#pragma once

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "homepi/log_rate_limiter.hpp"

namespace homepi::logging {

/** HomePi log severity levels. */
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

/**
 * Returns the string representation of a log level.
 * @param level Log level.
 * @return Level name.
 */
inline constexpr std::string_view level_name(LogLevel level) {
  switch (level) {
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARN:
      return "WARN";
    case LogLevel::ERROR:
      return "ERROR";
  }
  return "INFO";
}

/**
 * Escapes a string for JSON output.
 * @param value Raw string.
 * @return Escaped string.
 */
inline std::string escape_json_string(std::string_view value) {
  std::ostringstream out;
  for (char ch : value) {
    switch (ch) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\b':
        out << "\\b";
        break;
      case '\f':
        out << "\\f";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(static_cast<unsigned char>(ch)) << std::dec;
        } else {
          out << ch;
        }
    }
  }
  return out.str();
}

/**
 * Emits a single-line structured JSON log to stdout/stderr.
 */
class Logger {
 public:
  Logger(std::string service, LogLevel min_level = LogLevel::INFO)
      : service_(std::move(service)), min_level_(min_level) {}

  void log(LogLevel level, std::string_view module, std::string_view event,
           std::string_view correlation_id, std::string_view message,
           std::string_view data_json = "{}") {
    if (static_cast<int>(level) < static_cast<int>(min_level_)) {
      return;
    }
    if (!rate_limiter_.should_emit(std::string(event))) {
      return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ts;
    ts << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");

    std::ostringstream line;
    line << '{'
         << "\"ts\":\"" << ts.str() << '"'
         << ",\"service\":\"" << escape_json_string(service_) << '"'
         << ",\"module\":\"" << escape_json_string(module) << '"'
         << ",\"level\":\"" << level_name(level) << '"'
         << ",\"event\":\"" << escape_json_string(event) << '"'
         << ",\"correlationId\":\"" << escape_json_string(correlation_id) << '"'
         << ",\"message\":\"" << escape_json_string(message) << '"'
         << ",\"data\":" << data_json << '}';

    auto& stream =
        (level == LogLevel::WARN || level == LogLevel::ERROR) ? std::cerr : std::cout;
    stream << line.str() << '\n';
  }

 private:
  std::string service_;
  LogLevel min_level_;
  LogRateLimiter rate_limiter_;
};

}  // namespace homepi::logging
