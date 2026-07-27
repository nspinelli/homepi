#!/usr/bin/env node
import { mkdirSync, rmSync } from "node:fs";
import { dirname } from "node:path";

import { createLogger, createCorrelationId } from "@homepi/core-logging";
import { MessagingSocketServer, type SocketRequest } from "@homepi/core-messaging";

import { BrokerPublisher } from "./broker/broker-publisher.js";
import { ContactSensorService } from "./contact-sensor-service.js";
import { openSensorDatabase, serializeSensorRecord } from "./db/sensor-repository.js";
import { SensorRepository } from "./db/sensor-repository.js";
import { HomekitAccessoryClient } from "./homekit/homekit-accessory-client.js";
import type { ContactSensorPatch } from "./types/contact-sensor-types.js";

const SOCKET_PATH = process.env.HOMEPI_SENSORS_SOCKET ?? "/run/homepi/sensors/sensors.sock";
const DB_PATH =
  process.env.HOMEPI_STATE_DB ?? "/opt/homepi/runtime/state/homepi.sqlite";
const BROKER_SOCKET =
  process.env.HOMEPI_BROKER_SOCKET ?? "/run/homepi/broker/broker.sock";
const HOMEKIT_SOCKET =
  process.env.HOMEPI_HOMEKIT_SOCKET ?? "/run/homepi/homekit/homekit.sock";
const GPIO_CHIP = process.env.HOMEPI_GPIO_CHIP ?? "gpiochip4";

const logger = createLogger({ service: "homepi-sensors", minLevel: "INFO" });

const db = openSensorDatabase(DB_PATH);
const repository = new SensorRepository(db);
const broker = new BrokerPublisher(BROKER_SOCKET);
const homekit = new HomekitAccessoryClient(HOMEKIT_SOCKET);
const sensorService = new ContactSensorService({
  repository,
  logger,
  broker,
  homekit,
  gpioChip: GPIO_CHIP,
});

/**
 * Handles Contact Sensors facade commands.
 * @param request - Incoming socket request.
 * @returns Command result payload.
 */
async function handleCommand(request: SocketRequest): Promise<Record<string, unknown>> {
  switch (request.command) {
    case "ping":
      return { pong: true, service: "homepi-sensors" };
    case "getHealth":
    case "sensors.health":
      return sensorService.getHealth();
    case "sensors.snapshot":
      return sensorService.getSnapshot();
    case "sensors.sensor.get": {
      const sensorId = String(request.payload?.sensorId ?? request.payload?.id ?? "");
      const result = sensorService.getSensor(sensorId);
      if (!result) {
        throw new Error(`Unknown sensor: ${sensorId}`);
      }
      return result;
    }
    case "sensors.sensor.patch": {
      const sensorId = String(request.payload?.sensorId ?? request.payload?.id ?? "");
      const patch = (request.payload?.patch ?? request.payload ?? {}) as ContactSensorPatch;
      const updated = await sensorService.patchSensor(sensorId, {
        sensorName: patch.sensorName,
        sensorType: patch.sensorType,
        roomId: patch.roomId,
        roomName: patch.roomName,
        homekitEnabled: patch.homekitEnabled,
      });
      if (!updated) {
        throw new Error(`Unknown sensor: ${sensorId}`);
      }
      return { sensor: serializeSensorRecord(updated) };
    }
    case "sensors.diagnostics.get":
      return sensorService.getDiagnostics();
    default:
      throw new Error(`Unsupported sensors command: ${request.command}`);
  }
}

/**
 * Starts the Contact Sensors module facade.
 */
async function main(): Promise<void> {
  await sensorService.start();

  mkdirSync(dirname(SOCKET_PATH), { recursive: true });
  try {
    rmSync(SOCKET_PATH, { force: true });
  } catch {
    /* ignore */
  }

  const server = new MessagingSocketServer({
    socketPath: SOCKET_PATH,
    serviceName: "homepi-sensors",
    handlers: {
      ping: handleCommand,
      getHealth: handleCommand,
      "sensors.health": handleCommand,
      "sensors.snapshot": handleCommand,
      "sensors.sensor.get": handleCommand,
      "sensors.sensor.patch": handleCommand,
      "sensors.diagnostics.get": handleCommand,
    },
  });

  await server.start();

  logger.info({
    module: "sensors",
    event: "service_started",
    correlationId: createCorrelationId("startup"),
    message: "homepi-sensors facade listening",
    data: { socketPath: SOCKET_PATH, dbPath: DB_PATH },
  });

  const shutdown = async (): Promise<void> => {
    await server.stop();
    await sensorService.stop();
    db.close();
    rmSync(SOCKET_PATH, { force: true });
    process.exit(0);
  };

  process.on("SIGINT", () => {
    void shutdown();
  });
  process.on("SIGTERM", () => {
    void shutdown();
  });
}

main().catch((error: unknown) => {
  logger.error({
    module: "sensors",
    event: "service_failed",
    correlationId: createCorrelationId("fatal"),
    message: error instanceof Error ? error.message : String(error),
  });
  process.exit(1);
});
