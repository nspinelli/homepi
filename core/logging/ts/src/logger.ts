import type { LogInput, LogLevel, LogMessage, LoggerOptions } from "./log-types.js";
import { resolveCorrelationId } from "./correlation.js";
import { serializeLogMessage } from "./json-escape.js";
import { LogRateLimiter } from "./rate-limiter.js";

const LEVEL_ORDER: Record<LogLevel, number> = {
  DEBUG: 0,
  INFO: 1,
  WARN: 2,
  ERROR: 3,
};

/**
 * Writes a line to stdout or stderr per journald conventions.
 * @param level - Log level.
 * @param line - Single-line JSON log.
 */
function writeLogLine(level: LogLevel, line: string): void {
  const stream = level === "WARN" || level === "ERROR" ? process.stderr : process.stdout;
  stream.write(`${line}\n`);
}

/**
 * HomePi structured JSON logger.
 */
export class Logger {
  private readonly service: string;
  private readonly minLevel: LogLevel;
  private readonly rateLimiter: LogRateLimiter;

  /**
   * @param options - Logger configuration.
   */
  constructor(options: LoggerOptions) {
    this.service = options.service;
    this.minLevel = options.minLevel ?? "INFO";
    this.rateLimiter = new LogRateLimiter(
      options.rateLimitMaxPerWindow ?? 100,
      options.rateLimitWindowMs ?? 10_000
    );
  }

  /**
   * Emits a DEBUG log entry.
   * @param input - Log input fields.
   */
  debug(input: LogInput): void {
    this.log("DEBUG", input);
  }

  /**
   * Emits an INFO log entry.
   * @param input - Log input fields.
   */
  info(input: LogInput): void {
    this.log("INFO", input);
  }

  /**
   * Emits a WARN log entry.
   * @param input - Log input fields.
   */
  warn(input: LogInput): void {
    this.log("WARN", input);
  }

  /**
   * Emits an ERROR log entry.
   * @param input - Log input fields.
   */
  error(input: LogInput): void {
    this.log("ERROR", input);
  }

  /**
   * Builds a structured log message without writing it.
   * @param level - Log level.
   * @param input - Log input fields.
   * @returns Complete log message object.
   */
  buildMessage(level: LogLevel, input: LogInput): LogMessage {
    return {
      ts: new Date().toISOString(),
      service: this.service,
      module: input.module,
      level,
      event: input.event,
      correlationId: resolveCorrelationId(input.correlationId),
      message: input.message,
      data: input.data ?? {},
    };
  }

  /**
   * Emits a structured log at the given level.
   * @param level - Log level.
   * @param input - Log input fields.
   */
  log(level: LogLevel, input: LogInput): void {
    if (LEVEL_ORDER[level] < LEVEL_ORDER[this.minLevel]) {
      return;
    }

    if (!this.rateLimiter.shouldEmit(input.event)) {
      return;
    }

    const message = this.buildMessage(level, input);
    writeLogLine(level, serializeLogMessage(message as unknown as Record<string, unknown>));
  }
}

/**
 * Creates a logger for a HomePi service.
 * @param options - Logger configuration.
 * @returns Configured logger instance.
 */
export function createLogger(options: LoggerOptions): Logger {
  return new Logger(options);
}
