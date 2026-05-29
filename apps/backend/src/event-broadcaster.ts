import { createEventEnvelope } from "@homepi/core-events";
import type { EventEnvelope } from "@homepi/core-events";
import type { Logger } from "@homepi/core-logging";
import type { SystemStatusSnapshot } from "./types/system-status-types.js";

const EVENT_SOURCE = "homepi-backend";
const STATUS_TOPIC = "system.status";

/**
 * Manages SSE subscribers and emits HomePi event envelopes.
 */
export class EventBroadcaster {
  private readonly subscribers = new Set<ServerResponseLike>();
  private heartbeatTimer: ReturnType<typeof setInterval> | null = null;
  private statusTimer: ReturnType<typeof setInterval> | null = null;

  /**
   * Creates an event broadcaster.
   * @param logger - Structured logger instance.
   * @param getStatus - Callback returning the latest system status.
   */
  constructor(
    private readonly logger: Logger,
    private readonly getStatus: () => SystemStatusSnapshot
  ) {}

  /**
   * Registers an SSE subscriber and sends the initial snapshot.
   * @param res - HTTP response used for SSE.
   * @param correlationId - Request correlation identifier.
   */
  addSubscriber(res: ServerResponseLike, correlationId: string): void {
    this.subscribers.add(res);
    this.logger.info({
      module: "app.backend.events",
      event: "sse_client_connected",
      correlationId,
      message: "SSE client connected",
      data: { subscribers: this.subscribers.size },
    });

    this.sendEnvelope(res, this.createSnapshotEnvelope(correlationId));
  }

  /**
   * Removes an SSE subscriber.
   * @param res - HTTP response used for SSE.
   * @param correlationId - Request correlation identifier.
   */
  removeSubscriber(res: ServerResponseLike, correlationId: string): void {
    if (!this.subscribers.delete(res)) {
      return;
    }

    this.logger.info({
      module: "app.backend.events",
      event: "sse_client_disconnected",
      correlationId,
      message: "SSE client disconnected",
      data: { subscribers: this.subscribers.size },
    });
  }

  /**
   * Starts periodic heartbeat and status delta broadcasts.
   * @param onStatusEmit - Callback invoked when a status event is emitted.
   */
  start(onStatusEmit: (timestamp: string) => void): void {
    if (this.heartbeatTimer || this.statusTimer) {
      return;
    }

    this.heartbeatTimer = setInterval(() => {
      const envelope = createEventEnvelope({
        source: EVENT_SOURCE,
        topic: STATUS_TOPIC,
        event: "heartbeat",
        correlationId: `heartbeat-${Date.now()}`,
        payload: { kind: "heartbeat" },
      });
      this.broadcast(envelope);
    }, 30_000);

    this.statusTimer = setInterval(() => {
      const timestamp = new Date().toISOString();
      onStatusEmit(timestamp);
      this.broadcast(
        this.createDeltaEnvelope(`status-${Date.now()}`, timestamp)
      );
    }, 5_000);
  }

  /**
   * Stops periodic broadcasts and clears subscribers.
   */
  stop(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
    if (this.statusTimer) {
      clearInterval(this.statusTimer);
      this.statusTimer = null;
    }
    this.subscribers.clear();
  }

  /**
   * Broadcasts a validated event envelope to all SSE subscribers.
   * @param envelope - Event envelope to send.
   */
  broadcast(envelope: EventEnvelope): void {
    const frame = formatSseMessage(envelope);
    for (const subscriber of this.subscribers) {
      try {
        subscriber.write(frame);
      } catch {
        this.subscribers.delete(subscriber);
      }
    }
  }

  /**
   * Creates a system status snapshot event envelope.
   * @param correlationId - Correlation identifier.
   * @returns Event envelope.
   */
  createSnapshotEnvelope(correlationId: string): EventEnvelope {
    return createEventEnvelope({
      source: EVENT_SOURCE,
      topic: STATUS_TOPIC,
      event: "system_status_snapshot",
      correlationId,
      payload: {
        snapshot: this.getStatus(),
      },
    });
  }

  /**
   * Creates a system status delta event envelope.
   * @param correlationId - Correlation identifier.
   * @param emittedAt - Emission timestamp.
   * @returns Event envelope.
   */
  createDeltaEnvelope(correlationId: string, emittedAt: string): EventEnvelope {
    return createEventEnvelope({
      source: EVENT_SOURCE,
      topic: STATUS_TOPIC,
      event: "system_status_delta",
      correlationId,
      timestamp: emittedAt,
      payload: {
        status: this.getStatus(),
        emittedAt,
      },
    });
  }

  private sendEnvelope(res: ServerResponseLike, envelope: EventEnvelope): void {
    res.write(formatSseMessage(envelope));
  }
}

/**
 * Minimal response surface required for SSE streaming.
 */
export interface ServerResponseLike {
  write(chunk: string): void;
}

/**
 * Formats a HomePi event envelope as an SSE message frame.
 * @param envelope - Event envelope payload.
 * @returns SSE formatted string.
 */
export function formatSseMessage(envelope: EventEnvelope): string {
  return `event: envelope\ndata: ${JSON.stringify(envelope)}\n\n`;
}
