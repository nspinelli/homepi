import type { EventEnvelope } from "@homepi/core-events";

/** Broker topic patterns used for contact sensors UI SSE. */
export const BROKER_SENSORS_TOPICS = [
  "homepi.sensors.contact.changed",
  "homepi.sensors.contact.config_updated",
  "homepi.sensors.contact.faulted",
  "homepi.homekit.bridge.ready",
  "homepi.homekit.accessory.updated",
  "homepi.homekit.accessory.removed",
] as const;

/**
 * Maps broker sensor events to SSE envelope names the frontend handles.
 * @param envelope - Raw broker envelope.
 * @returns Adapted envelope for UI consumers.
 */
export function adaptSensorsBrokerEnvelopeForUi(envelope: EventEnvelope): EventEnvelope {
  const adapted: EventEnvelope = { ...envelope };

  if (envelope.topic === "homepi.sensors.contact.changed") {
    adapted.event = "contact_changed";
    adapted.source = "homepi-sensors";
  }

  if (envelope.topic === "homepi.sensors.contact.config_updated") {
    adapted.event = "contact_config_updated";
    adapted.source = "homepi-sensors";
  }

  if (envelope.topic === "homepi.sensors.contact.faulted") {
    adapted.event = "contact_faulted";
    adapted.source = "homepi-sensors";
  }

  return adapted;
}
