import type { Logger } from "@homepi/core-logging";

/** Exponential backoff delays in milliseconds (max 30s). */
export const EVENT_BRIDGE_BACKOFF_MS = [1_000, 2_000, 5_000, 10_000, 30_000] as const;

/**
 * Options for socket event bridge reconnect scheduling.
 */
export interface EventBridgeReconnectOptions {
  /** Structured logger. */
  logger: Logger;
  /** Log module identifier. */
  module: string;
  /** Bridge correlation id for logs. */
  correlationId: string;
  /** Callback to attempt a new connection. */
  connect: () => void;
  /** Returns true when the bridge has been stopped. */
  isStopped: () => boolean;
}

/**
 * Manages exponential backoff reconnect for native Unix socket event bridges.
 */
export class EventBridgeReconnect {
  private attempt = 0;
  private timer: ReturnType<typeof setTimeout> | null = null;

  /**
   * @param options - Reconnect configuration.
   */
  constructor(private readonly options: EventBridgeReconnectOptions) {}

  /**
   * Resets backoff after a successful connection.
   */
  resetBackoff(): void {
    this.attempt = 0;
  }

  /**
   * Clears any pending reconnect timer.
   */
  clearTimer(): void {
    if (this.timer) {
      clearTimeout(this.timer);
      this.timer = null;
    }
  }

  /**
   * Schedules the next reconnect attempt with exponential backoff.
   */
  scheduleReconnect(): void {
    if (this.options.isStopped() || this.timer) {
      return;
    }

    const delay =
      EVENT_BRIDGE_BACKOFF_MS[Math.min(this.attempt, EVENT_BRIDGE_BACKOFF_MS.length - 1)];
    this.attempt += 1;

    this.options.logger.info({
      module: this.options.module,
      event: "event_bridge_reconnect_scheduled",
      correlationId: this.options.correlationId,
      message: "Event bridge reconnect scheduled",
      data: { delayMs: delay, attempt: this.attempt },
    });

    this.timer = setTimeout(() => {
      this.timer = null;
      if (this.options.isStopped()) {
        return;
      }
      this.options.logger.info({
        module: this.options.module,
        event: "event_bridge_connecting",
        correlationId: this.options.correlationId,
        message: "Event bridge reconnecting",
      });
      try {
        this.options.connect();
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        this.options.logger.warn({
          module: this.options.module,
          event: "event_bridge_reconnect_failed",
          correlationId: this.options.correlationId,
          message: "Event bridge reconnect failed",
          data: { error: message },
        });
        this.scheduleReconnect();
      }
    }, delay);
  }
}
