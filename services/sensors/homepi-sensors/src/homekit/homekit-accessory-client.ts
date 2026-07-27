import { sendCommand } from "@homepi/core-messaging";

import type { ContactSensorRecord } from "../types/contact-sensor-types.js";
import { serializeSensorRecord } from "../db/sensor-repository.js";

const SOURCE = "homepi-sensors";

/**
 * Stable HomeKit accessory UUID per sensor id (spec §15.3).
 * @param sensorId - Sensor id.
 * @returns UUID string.
 */
export function stableHomekitUuid(sensorId: string): string {
  const hex = sensorId.replace(/[^a-f0-9]/gi, "").padEnd(12, "0").slice(0, 12);
  return `00000000-0000-4000-8000-${hex.padStart(12, "0")}`;
}

/**
 * Thin client for the platform homepi-homekit bridge.
 */
export class HomekitAccessoryClient {
  /**
   * Creates a HomeKit accessory client.
   * @param socketPath - homepi-homekit socket path.
   */
  constructor(private readonly socketPath: string) {}

  /**
   * Returns whether the HomeKit bridge socket responds to ping.
   * @returns True when bridge is reachable.
   */
  async isReachable(): Promise<boolean> {
    try {
      const response = await sendCommand(
        this.socketPath,
        SOURCE,
        "homepi-homekit",
        "ping",
        {},
        2_000
      );
      return response.ok === true;
    } catch {
      return false;
    }
  }

  /**
   * Registers or updates a contact sensor accessory.
   * @param sensor - Sensor record.
   */
  async syncContactSensor(sensor: ContactSensorRecord): Promise<void> {
    if (!sensor.homekitEnabled) {
      await this.removeAccessory(sensor.sensorId);
      return;
    }

    await sendCommand(this.socketPath, SOURCE, "homepi-homekit", "homekit.accessory.register", {
      moduleId: "contact-sensors",
      accessoryType: "ContactSensor",
      stableUuid: stableHomekitUuid(sensor.sensorId),
      displayName: sensor.sensorName,
      state: {
        contactDetected: sensor.contactState === "open",
      },
      metadata: serializeSensorRecord(sensor),
    });
  }

  /**
   * Pushes a state update for an enabled accessory.
   * @param sensor - Sensor record.
   */
  async updateContactSensor(sensor: ContactSensorRecord): Promise<void> {
    if (!sensor.homekitEnabled) {
      return;
    }

    await sendCommand(this.socketPath, SOURCE, "homepi-homekit", "homekit.accessory.update", {
      stableUuid: stableHomekitUuid(sensor.sensorId),
      state: {
        contactDetected: sensor.contactState === "open",
      },
    });
  }

  /**
   * Removes a sensor accessory from HomeKit.
   * @param sensorId - Sensor id.
   */
  async removeAccessory(sensorId: string): Promise<void> {
    try {
      await sendCommand(this.socketPath, SOURCE, "homepi-homekit", "homekit.accessory.remove", {
        stableUuid: stableHomekitUuid(sensorId),
      });
    } catch {
      /* bridge may be offline */
    }
  }
}
