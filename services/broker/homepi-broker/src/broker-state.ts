import type { BrokerEvent, SocketRequest } from "@homepi/core-messaging";
import { createBrokerEvent, createSuccessResponse } from "@homepi/core-messaging";

/**
 * In-memory broker state for publish/subscribe fanout.
 */
export class BrokerState {
  private readonly subscribers = new Map<string, Set<(event: BrokerEvent) => void>>();
  private readonly recentEvents: BrokerEvent[] = [];
  private readonly maxRecent = 500;

  /**
   * Registers a topic subscription callback.
   * @param topics - Topic patterns to subscribe to.
   * @param listener - Callback invoked for matching events.
   * @returns Unsubscribe function.
   */
  subscribe(topics: string[], listener: (event: BrokerEvent) => void): () => void {
    for (const topic of topics) {
      const set = this.subscribers.get(topic) ?? new Set();
      set.add(listener);
      this.subscribers.set(topic, set);
    }

    return () => {
      for (const topic of topics) {
        this.subscribers.get(topic)?.delete(listener);
      }
    };
  }

  /**
   * Publishes an event to matching subscribers.
   * @param event - Broker event envelope.
   */
  publish(event: BrokerEvent): void {
    this.recentEvents.unshift(event);
    if (this.recentEvents.length > this.maxRecent) {
      this.recentEvents.length = this.maxRecent;
    }

    for (const [topic, listeners] of this.subscribers) {
      if (this.topicMatches(topic, event.topic)) {
        for (const listener of listeners) {
          listener(event);
        }
      }
    }
  }

  /**
   * Returns recent events optionally filtered by topic prefix.
   * @param topicPrefix - Optional topic prefix filter.
   * @returns Recent broker events newest-first.
   */
  snapshot(topicPrefix?: string): BrokerEvent[] {
    if (!topicPrefix) {
      return [...this.recentEvents];
    }
    return this.recentEvents.filter((event) => event.topic.startsWith(topicPrefix));
  }

  /**
   * Handles a broker command request.
   * @param request - Incoming socket request.
   * @returns Command result payload.
   */
  handleCommand(request: SocketRequest): Record<string, unknown> {
    switch (request.command) {
      case "ping":
        return { pong: true, service: "homepi-broker" };
      case "publish": {
        const payload = request.payload ?? {};
        const event = createBrokerEvent({
          topic: String(payload.topic ?? ""),
          source: String(payload.source ?? request.source),
          correlationId: request.correlationId,
          severity: (payload.severity as BrokerEvent["severity"]) ?? "info",
          payload: (payload.eventPayload as Record<string, unknown>) ?? {},
          uiVisible: payload.uiVisible as boolean | undefined,
        });
        this.publish(event);
        return { published: true, id: event.id };
      }
      case "subscribe":
        return { subscribed: true, topics: request.payload?.topics ?? [] };
      case "unsubscribe":
        return { unsubscribed: true, topics: request.payload?.topics ?? [] };
      case "snapshot":
        return {
          events: this.snapshot(
            typeof request.payload?.topicPrefix === "string"
              ? request.payload.topicPrefix
              : undefined
          ),
        };
      default:
        throw new Error(`Unsupported broker command: ${request.command}`);
    }
  }

  private topicMatches(subscription: string, topic: string): boolean {
    if (subscription.endsWith("*")) {
      return topic.startsWith(subscription.slice(0, -1));
    }
    return subscription === topic || subscription === "homepi.*";
  }
}

/**
 * Creates a success response helper for broker commands.
 * @param request - Original request.
 * @param result - Result payload.
 * @returns Response envelope object.
 */
export function brokerSuccess(
  request: SocketRequest,
  result: Record<string, unknown>
): ReturnType<typeof createSuccessResponse> {
  return createSuccessResponse(request, result);
}
