import { createEventEnvelope } from "@homepi/core-events";
import type { EventEnvelope } from "@homepi/core-events";
import type { Logger } from "@homepi/core-logging";
import type { SystemStatusSnapshot } from "./types/system-status-types.js";

const EVENT_SOURCE = "homepi-backend";
const STATUS_TOPIC = "system.status";
const MAX_REPLAY_EVENTS = 200;

/**
 * Manages SSE subscribers and emits HomePi event envelopes.
 */
export class EventBroadcaster {
  private readonly subscribers = new Set<ServerResponseLike>();
  private readonly recentEnvelopes: EventEnvelope[] = [];
  private heartbeatTimer: ReturnType<typeof setInterval> | null = null;

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
    for (const envelope of this.recentEnvelopes) {
      this.sendEnvelope(res, envelope);
    }
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
   * Starts periodic SSE heartbeat broadcasts (transport liveness only).
   */
  start(): void {
    if (this.heartbeatTimer) {
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
  }

  /**
   * Broadcasts a system status delta to all SSE subscribers.
   * @param correlationId - Correlation identifier.
   * @param emittedAt - Optional emission timestamp.
   */
  broadcastStatusDelta(correlationId?: string, emittedAt?: string): void {
    const timestamp = emittedAt ?? new Date().toISOString();
    this.broadcast(
      this.createDeltaEnvelope(correlationId ?? `status-${Date.now()}`, timestamp)
    );
  }

  /**
   * Stops periodic broadcasts and clears subscribers.
   */
  stop(): void {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
    this.subscribers.clear();
    this.recentEnvelopes.length = 0;
  }

  /**
   * Broadcasts a validated event envelope to all SSE subscribers.
   * @param envelope - Event envelope to send.
   */
  broadcast(envelope: EventEnvelope): void {
    if (envelope.event !== "heartbeat") {
      this.recentEnvelopes.push(envelope);
      if (this.recentEnvelopes.length > MAX_REPLAY_EVENTS) {
        this.recentEnvelopes.shift();
      }
    }

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
