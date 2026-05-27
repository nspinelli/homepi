/**
 * Supported HomePi log levels per core-logging contract.
 */
export type LogLevel = "DEBUG" | "INFO" | "WARN" | "ERROR";

/**
 * Structured log message shape per log-message.schema.json.
 */
export interface LogMessage {
  /** ISO8601 UTC timestamp. */
  ts: string;
  /** Systemd service name (homepi-*). */
  service: string;
  /** Logical module name (dot-separated). */
  module: string;
  /** Log severity level. */
  level: LogLevel;
  /** Stable snake_case event identifier. */
  event: string;
  /** Operation trace identifier. */
  correlationId: string;
  /** Human-readable summary. */
  message: string;
  /** Structured event-specific payload. */
  data: Record<string, unknown>;
}

/**
 * Input fields required to emit a log entry.
 */
export interface LogInput {
  /** Logical module name. */
  module: string;
  /** Stable event identifier. */
  event: string;
  /** Correlation ID; generated when omitted. */
  correlationId?: string;
  /** Human-readable summary. */
  message: string;
  /** Structured payload; defaults to empty object. */
  data?: Record<string, unknown>;
}

/**
 * Logger configuration options.
 */
export interface LoggerOptions {
  /** Systemd service name. */
  service: string;
  /** Minimum level to emit (inclusive). */
  minLevel?: LogLevel;
  /** Rate limit: max identical events per window. */
  rateLimitMaxPerWindow?: number;
  /** Rate limit window duration in milliseconds. */
  rateLimitWindowMs?: number;
}
