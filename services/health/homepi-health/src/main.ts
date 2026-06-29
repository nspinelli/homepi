#!/usr/bin/env node
import { mkdirSync, rmSync } from "node:fs";
import { dirname } from "node:path";

import { createLogger, createCorrelationId } from "@homepi/core-logging";
import {
  MessagingSocketServer,
  type SocketRequest,
} from "@homepi/core-messaging";

import { buildSystemHealthSnapshot } from "./health-snapshot-builder.js";

const SOCKET_PATH = process.env.HOMEPI_HEALTH_SOCKET ?? "/run/homepi/health/health.sock";
const POLL_MS = Number(process.env.HOMEPI_HEALTH_POLL_MS ?? 30_000);
const logger = createLogger({ service: "homepi-health", minLevel: "INFO" });

let cachedSnapshot: Awaited<ReturnType<typeof buildSystemHealthSnapshot>>;

/**
 * Refreshes the cached health snapshot.
 */
async function refreshSnapshot(correlationId?: string): Promise<void> {
  cachedSnapshot = await buildSystemHealthSnapshot(correlationId ?? createCorrelationId("poll"));
}

/**
 * Handles health service commands.
 * @param request - Incoming socket request.
 * @returns Command result payload.
 */
async function handleCommand(request: SocketRequest): Promise<Record<string, unknown>> {
  switch (request.command) {
    case "ping":
      return { pong: true, service: "homepi-health" };
    case "health.snapshot":
      await refreshSnapshot(request.correlationId);
      return { snapshot: cachedSnapshot };
    case "health.module.get": {
      await refreshSnapshot(request.correlationId);
      const moduleId = String(request.payload?.module ?? "");
      const module = cachedSnapshot.modules.find((entry) => entry.module === moduleId);
      if (!module) {
        throw new Error(`Unknown module: ${moduleId}`);
      }
      return { module };
    }
    case "health.service.get": {
      await refreshSnapshot(request.correlationId);
      const serviceName = String(request.payload?.service ?? "");
      const service = cachedSnapshot.services.find((entry) => entry.service === serviceName);
      if (!service) {
        throw new Error(`Unknown service: ${serviceName}`);
      }
      return { service };
    }
    default:
      throw new Error(`Unknown command: ${request.command}`);
  }
}

/**
 * Starts the homepi-health observer daemon.
 */
async function main(): Promise<void> {
  cachedSnapshot = await buildSystemHealthSnapshot(createCorrelationId("boot"));

  mkdirSync(dirname(SOCKET_PATH), { recursive: true });
  try {
    rmSync(SOCKET_PATH, { force: true });
  } catch {
    /* ignore */
  }

  const server = new MessagingSocketServer({
    socketPath: SOCKET_PATH,
    serviceName: "homepi-health",
    handlers: {
      ping: handleCommand,
      "health.snapshot": handleCommand,
      "health.module.get": handleCommand,
      "health.service.get": handleCommand,
    },
  });

  await server.start();

  const pollTimer = setInterval(() => {
    void refreshSnapshot(createCorrelationId("poll")).catch((error: unknown) => {
      logger.warn({
        module: "health",
        event: "snapshot_poll_failed",
        correlationId: createCorrelationId("poll"),
        message: error instanceof Error ? error.message : String(error),
      });
    });
  }, POLL_MS);

  logger.info({
    module: "health",
    event: "service_started",
    correlationId: createCorrelationId("startup"),
    message: "homepi-health listening",
    data: { socketPath: SOCKET_PATH, pollMs: POLL_MS },
  });

  const shutdown = async (): Promise<void> => {
    clearInterval(pollTimer);
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
    module: "health",
    event: "service_failed",
    correlationId: createCorrelationId("fatal"),
    message: error instanceof Error ? error.message : String(error),
  });
  process.exit(1);
});
