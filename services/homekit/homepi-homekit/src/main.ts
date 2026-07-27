#!/usr/bin/env node
import { mkdirSync, rmSync } from "node:fs";
import { dirname } from "node:path";

import { createLogger, createCorrelationId } from "@homepi/core-logging";
import { MessagingSocketServer, type SocketRequest, sendCommand } from "@homepi/core-messaging";

import { AccessoryRegistry } from "./accessory-registry.js";

const SOCKET_PATH = process.env.HOMEPI_HOMEKIT_SOCKET ?? "/run/homepi/homekit/homekit.sock";
const STATE_DIR =
  process.env.HOMEPI_HOMEKIT_STATE_DIR ?? "/opt/homepi/runtime/state/homekit";
const BROKER_SOCKET =
  process.env.HOMEPI_BROKER_SOCKET ?? "/run/homepi/broker/broker.sock";

const logger = createLogger({ service: "homepi-homekit", minLevel: "INFO" });
const registry = new AccessoryRegistry(STATE_DIR);

/**
 * Publishes a broker event from the HomeKit bridge.
 * @param topic - Broker topic.
 * @param eventName - Legacy event name.
 * @param payload - Event payload.
 */
async function publishBrokerEvent(
  topic: string,
  eventName: string,
  payload: Record<string, unknown>
): Promise<void> {
  try {
    await sendCommand(BROKER_SOCKET, "homepi-homekit", "homepi-broker", "publish", {
      topic,
      source: "homepi-homekit",
      eventPayload: { event: eventName, ...payload },
      uiVisible: true,
    });
  } catch {
    /* broker optional during startup */
  }
}

/**
 * Handles HomeKit bridge facade commands.
 * @param request - Incoming socket request.
 * @returns Command result payload.
 */
async function handleCommand(request: SocketRequest): Promise<Record<string, unknown>> {
  switch (request.command) {
    case "ping":
      return { pong: true, service: "homepi-homekit" };
    case "getHealth":
    case "homekit.bridge.getHealth":
      return {
        module: "homekit",
        status: registry.isReady() ? "healthy" : "degraded",
        bridgeReady: registry.isReady(),
        accessoryCount: registry.list().length,
        capabilities: [
          {
            id: "homekit-bridge",
            status: registry.isReady() ? "healthy" : "offline",
          },
        ],
      };
    case "homekit.accessory.register": {
      const payload = request.payload ?? {};
      registry.register({
        moduleId: String(payload.moduleId ?? ""),
        accessoryType: String(payload.accessoryType ?? ""),
        stableUuid: String(payload.stableUuid ?? ""),
        displayName: String(payload.displayName ?? "Sensor"),
        state: (payload.state as Record<string, unknown>) ?? {},
        metadata: payload.metadata as Record<string, unknown> | undefined,
      });
      await publishBrokerEvent("homepi.homekit.accessory.updated", "homekit_accessory_updated", {
        stableUuid: payload.stableUuid,
      });
      return { registered: true };
    }
    case "homekit.accessory.update": {
      const payload = request.payload ?? {};
      registry.update(String(payload.stableUuid ?? ""), {
        displayName:
          typeof payload.displayName === "string" ? payload.displayName : undefined,
        state: payload.state as Record<string, unknown> | undefined,
      });
      await publishBrokerEvent("homepi.homekit.accessory.updated", "homekit_accessory_updated", {
        stableUuid: payload.stableUuid,
      });
      return { updated: true };
    }
    case "homekit.accessory.remove": {
      const stableUuid = String(request.payload?.stableUuid ?? "");
      registry.remove(stableUuid);
      await publishBrokerEvent("homepi.homekit.accessory.removed", "homekit_accessory_removed", {
        stableUuid,
      });
      return { removed: true };
    }
    case "homekit.accessory.list":
      return { accessories: registry.list() };
    default:
      throw new Error(`Unsupported homekit command: ${request.command}`);
  }
}

/**
 * Starts the HomeKit platform bridge service.
 */
async function main(): Promise<void> {
  await publishBrokerEvent("homepi.homekit.bridge.ready", "homekit_bridge_ready", {});

  mkdirSync(dirname(SOCKET_PATH), { recursive: true });
  try {
    rmSync(SOCKET_PATH, { force: true });
  } catch {
    /* ignore */
  }

  const server = new MessagingSocketServer({
    socketPath: SOCKET_PATH,
    serviceName: "homepi-homekit",
    handlers: {
      ping: handleCommand,
      getHealth: handleCommand,
      "homekit.bridge.getHealth": handleCommand,
      "homekit.accessory.register": handleCommand,
      "homekit.accessory.update": handleCommand,
      "homekit.accessory.remove": handleCommand,
      "homekit.accessory.list": handleCommand,
    },
  });

  await server.start();

  logger.info({
    module: "homekit",
    event: "service_started",
    correlationId: createCorrelationId("startup"),
    message: "homepi-homekit bridge listening",
    data: { socketPath: SOCKET_PATH, stateDir: STATE_DIR },
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
    module: "homekit",
    event: "service_failed",
    correlationId: createCorrelationId("fatal"),
    message: error instanceof Error ? error.message : String(error),
  });
  process.exit(1);
});
