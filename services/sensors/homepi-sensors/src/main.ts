#!/usr/bin/env node
import { mkdirSync, rmSync } from "node:fs";
import { dirname } from "node:path";

import { createLogger, createCorrelationId } from "@homepi/core-logging";
import { MessagingSocketServer, type SocketRequest } from "@homepi/core-messaging";

const SOCKET_PATH = process.env.HOMEPI_SENSORS_SOCKET ?? "/run/homepi/sensors/sensors.sock";
const logger = createLogger({ service: "homepi-sensors", minLevel: "INFO" });

/** In-memory placeholder sensor state until GPIO hardware integration ships. */
const sensorState = {
  contacts: [] as Array<{ id: string; name: string; open: boolean }>,
  homekitBridgeRunning: false,
};

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
      return {
        module: "contact-sensors",
        status: "degraded",
        userMessage:
          "Contact sensor hardware integration is starting. GPIO and HomeKit bridge wiring is not complete yet.",
        capabilities: [
          { id: "contact-detection", status: "offline" },
          { id: "tamper-fault", status: "offline" },
          {
            id: "homekit-bridge",
            status: sensorState.homekitBridgeRunning ? "healthy" : "offline",
            userMessage: sensorState.homekitBridgeRunning
              ? undefined
              : "HomeKit bridge is offline, but HomePi modules are still running.",
          },
        ],
      };
    case "sensors.snapshot":
      return { sensors: sensorState.contacts };
    default:
      throw new Error(`Unsupported sensors command: ${request.command}`);
  }
}

/**
 * Starts the Contact Sensors module facade.
 */
async function main(): Promise<void> {
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
    },
  });

  await server.start();

  logger.info({
    module: "sensors",
    event: "service_started",
    correlationId: createCorrelationId("startup"),
    message: "homepi-sensors facade listening",
    data: { socketPath: SOCKET_PATH },
  });

  const shutdown = async (): Promise<void> => {
    await server.stop();
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
