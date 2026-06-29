import type { EventEnvelope } from "@homepi/core-events";
import type { BrokerEvent } from "@homepi/core-messaging";

/**
 * Converts a v2 broker event wire envelope to the legacy SSE event shape.
 * @param brokerEvent - Broker event from homepi-broker.
 * @returns Legacy event envelope or null when required fields are missing.
 */
export function brokerEventToEventEnvelope(brokerEvent: BrokerEvent): EventEnvelope | null {
  if (!brokerEvent.topic || !brokerEvent.source || !brokerEvent.id || !brokerEvent.ts) {
    return null;
  }

  const payload = { ...(brokerEvent.payload ?? {}) };
  const eventName =
    typeof payload.event === "string" && payload.event.length > 0
      ? payload.event
      : inferEventName(brokerEvent.topic);

  if (typeof payload.event === "string") {
    delete payload.event;
  }

  return {
    version: 1,
    id: brokerEvent.id,
    source: brokerEvent.source,
    topic: brokerEvent.topic,
    event: eventName,
    correlationId: brokerEvent.correlationId,
    timestamp: brokerEvent.ts,
    payload,
  };
}

/**
 * Parses a broker wire line into a legacy event envelope.
 * @param line - Raw NDJSON line from homepi-broker.
 * @returns Legacy envelope when the line is a broker event.
 */
export function parseBrokerWireLine(line: string): EventEnvelope | null {
  let parsed: unknown;
  try {
    parsed = JSON.parse(line);
  } catch {
    return null;
  }

  if (
    typeof parsed !== "object" ||
    parsed === null ||
    (parsed as { type?: unknown }).type !== "event"
  ) {
    return null;
  }

  const brokerEvent = (parsed as { event?: BrokerEvent }).event;
  if (!brokerEvent || typeof brokerEvent !== "object") {
    return null;
  }

  return brokerEventToEventEnvelope(brokerEvent);
}

/**
 * Infers a legacy event name from a broker topic when publishers omit it.
 * @param topic - Broker topic string.
 * @returns Best-effort legacy event name.
 */
function inferEventName(topic: string): string {
  const segments = topic.split(".");
  return segments[segments.length - 1] ?? "event";
}
