#!/usr/bin/env node
import { mkdirSync, rmSync } from "node:fs";
import { dirname } from "node:path";

import { createLogger, createCorrelationId } from "@homepi/core-logging";
import { MessagingSocketServer, type SocketRequest } from "@homepi/core-messaging";

import { getAudioCapabilityHealth, proxyLegacy } from "./audio-internal-proxy.js";

const SOCKET_PATH = process.env.HOMEPI_AUDIO_SOCKET ?? "/run/homepi/audio/audio.sock";
const logger = createLogger({ service: "homepi-audio", minLevel: "INFO" });

/**
 * Handles facade commands by proxying to internal services.
 * @param request - Incoming socket request.
 * @returns Command result payload.
 */
async function handleCommand(request: SocketRequest): Promise<Record<string, unknown>> {
  switch (request.command) {
    case "ping":
      return { pong: true, service: "homepi-audio" };
    case "audio.health":
    case "getHealth":
      return getAudioCapabilityHealth();
    case "hifi.zone.setPower":
      return proxyLegacy("homepi-hifi-serial", "setZonePower", request.payload ?? {});
    case "hifi.zone.setVolume":
      return proxyLegacy("homepi-hifi-serial", "setZoneVolume", request.payload ?? {});
    case "hifi.zone.setSource":
      return proxyLegacy("homepi-hifi-serial", "setZoneSource", request.payload ?? {});
    case "pcm.setZoneEnabled":
      return proxyLegacy("homepi-pcm-router", "set_zone_enabled", request.payload ?? {});
    case "paging.start":
      return proxyLegacy("homepi-audio-paging", "startPaging", request.payload ?? {});
    case "paging.stop":
      return proxyLegacy("homepi-audio-paging", "stopPaging", request.payload ?? {});
    default:
      throw new Error(`Unsupported audio command: ${request.command}`);
  }
}

/**
 * Starts the Home Audio module facade.
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
    serviceName: "homepi-audio",
    handlers: {
      ping: handleCommand,
      "audio.health": handleCommand,
      getHealth: handleCommand,
      "hifi.zone.setPower": handleCommand,
      "hifi.zone.setVolume": handleCommand,
      "hifi.zone.setSource": handleCommand,
      "pcm.setZoneEnabled": handleCommand,
      "paging.start": handleCommand,
      "paging.stop": handleCommand,
    },
  });

  await server.start();

  logger.info({
    module: "audio",
    event: "service_started",
    correlationId: createCorrelationId("startup"),
    message: "homepi-audio facade listening",
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
    module: "audio",
    event: "service_failed",
    correlationId: createCorrelationId("fatal"),
    message: error instanceof Error ? error.message : String(error),
  });
  process.exit(1);
});
