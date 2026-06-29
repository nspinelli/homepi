import { createEventEnvelope } from "@homepi/core-events";
import type { EventEnvelope } from "@homepi/core-events";
import type { Logger } from "@homepi/core-logging";
import type { SystemStatusSnapshot } from "./types/system-status-types.js";

const EVENT_SOURCE = "homepi-backend";
const STATUS_TOPIC = "system.status";
const MAX_REPLAY_EVENTS = 200;

/**
 * Returns true when an envelope must not be buffered or replayed to late SSE subscribers.
 * @param envelope - Event envelope.
 * @returns True when replay could wipe live UI state.
 */
function shouldExcludeFromReplay(envelope: EventEnvelope): boolean {
  if (envelope.source === "homepi-metadata") {
    return true;
  }
  if (envelope.source === "homepi-backend" && envelope.event === "audio.realtime") {
    return true;
  }
  return false;
}

/**
 * Supplies fresh envelopes when a new SSE client connects.
 * @param correlationId - Request correlation identifier.
 * @returns Envelopes to send after the system status snapshot.
 */
export type SseSubscribeBootstrap = (
  correlationId: string
) => Promise<EventEnvelope[]>;

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
   * @param subscribeBootstrap - Optional provider of fresh envelopes on connect.
   */
  constructor(
    private readonly logger: Logger,
    private readonly getStatus: () => SystemStatusSnapshot,
    private readonly subscribeBootstrap?: SseSubscribeBootstrap
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
      if (!shouldExcludeFromReplay(envelope)) {
        this.sendEnvelope(res, envelope);
      }
    }

    if (this.subscribeBootstrap) {
      void this.subscribeBootstrap(correlationId)
        .then((envelopes) => {
          if (!this.subscribers.has(res)) {
            return;
          }
          for (const envelope of envelopes) {
            this.sendEnvelope(res, envelope);
          }
        })
        .catch((error: unknown) => {
          this.logger.warn({
            module: "app.backend.events",
            event: "sse_subscribe_bootstrap_failed",
            correlationId,
            message: "Failed to bootstrap SSE subscriber",
            data: {
              error: error instanceof Error ? error.message : String(error),
            },
          });
        });
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
        payload: { kind: "heartbeat", uiVisible: false },
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
    if (envelope.event !== "heartbeat" && !shouldExcludeFromReplay(envelope)) {
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
