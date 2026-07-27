import { createCorrelationId } from "@homepi/core-logging";
import { createBrokerEvent, sendCommand } from "@homepi/core-messaging";

import type { ContactSensorRecord } from "../types/contact-sensor-types.js";
import { serializeSensorRecord } from "../db/sensor-repository.js";

const SOURCE = "homepi-sensors";

/**
 * Publishes contact sensor events to homepi-broker.
 */
export class BrokerPublisher {
  /**
   * Creates a broker publisher.
   * @param brokerSocketPath - Unix socket path for homepi-broker.
   */
  constructor(private readonly brokerSocketPath: string) {}

  /**
   * Publishes a contact state change event.
   * @param sensor - Updated sensor record.
   */
  async publishContactChanged(sensor: ContactSensorRecord): Promise<void> {
    await this.publish("homepi.sensors.contact.changed", "contact_changed", {
      sensor: serializeSensorRecord(sensor),
    });
  }

  /**
   * Publishes a configuration update event.
   * @param sensor - Updated sensor record.
   */
  async publishConfigUpdated(sensor: ContactSensorRecord): Promise<void> {
    await this.publish("homepi.sensors.contact.config_updated", "contact_config_updated", {
      sensor: serializeSensorRecord(sensor),
    });
  }

  /**
   * Publishes a fault event.
   * @param sensor - Faulted sensor record.
   */
  async publishFaulted(sensor: ContactSensorRecord): Promise<void> {
    await this.publish("homepi.sensors.contact.faulted", "contact_faulted", {
      sensor: serializeSensorRecord(sensor),
    });
  }

  private async publish(
    topic: string,
    eventName: string,
    eventPayload: Record<string, unknown>
  ): Promise<void> {
    const correlationId = createCorrelationId("sensors");
    const event = createBrokerEvent({
      topic,
      source: SOURCE,
      correlationId,
      payload: { event: eventName, ...eventPayload },
      uiVisible: true,
    });

    try {
      await sendCommand(
        this.brokerSocketPath,
        SOURCE,
        "homepi-broker",
        "publish",
        {
          topic: event.topic,
          source: event.source,
          severity: event.severity,
          eventPayload: event.payload,
          uiVisible: event.uiVisible,
        }
      );
    } catch {
      /* broker may be offline during startup */
    }
  }
}
