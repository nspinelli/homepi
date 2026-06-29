import { createEventId } from "./correlation.js";
import type { BrokerEvent } from "./messaging-types.js";

/**
 * Input for creating a broker event.
 */
export interface CreateBrokerEventInput {
  /** Event topic. */
  topic: string;
  /** Publishing service. */
  source: string;
  /** Correlation id. */
  correlationId: string;
  /** Event severity. */
  severity?: BrokerEvent["severity"];
  /** Event payload. */
  payload: Record<string, unknown>;
  /** Whether the event is user-visible in the status log. */
  uiVisible?: boolean;
}

/**
 * Creates a broker event envelope.
 * @param input - Event fields.
 * @returns Broker event object.
 */
export function createBrokerEvent(input: CreateBrokerEventInput): BrokerEvent {
  return {
    v: 1,
    id: createEventId(),
    ts: new Date().toISOString(),
    topic: input.topic,
    source: input.source,
    correlationId: input.correlationId,
    severity: input.severity ?? "info",
    payload: input.payload,
    ...(input.uiVisible === false ? { uiVisible: false } : {}),
  };
}

/**
 * Returns true when an event should appear in the status activity log.
 * @param event - Broker or legacy event envelope.
 * @returns Whether the event is user-visible.
 */
export function isUiVisibleEvent(event: {
  event?: string;
  topic?: string;
  uiVisible?: boolean;
}): boolean {
  if (event.uiVisible === false) {
    return false;
  }

  const eventName = event.event ?? "";
  if (
    eventName === "heartbeat" ||
    eventName === "system_status_snapshot" ||
    eventName === "system_status_delta" ||
    eventName === "audio.realtime" ||
    eventName === "metadata_progress_updated"
  ) {
    return false;
  }

  return true;
}
