import type { IncomingMessage, ServerResponse } from "node:http";

import type { ServiceConfig } from "@homepi/core-config";
import type { Logger } from "@homepi/core-logging";
import { createErrorResponse, createSuccessResponse } from "@homepi/core-api";

import type { HifiSerialClient } from "../hifi-serial/hifi-serial-client.js";
import type { PcmRouterClient } from "../pcm-router/pcm-router-client.js";
import type { SystemStatusStore } from "../system-status-store.js";
import { buildAudioSnapshot } from "./build-audio-snapshot.js";
import {
  buildZoneControllerCommands,
  requiresShairportRestart,
  type ShairportZonePatch,
  type ZoneControllerPatch,
} from "./hifi-zone-commands.js";

/**
 * Dependencies for audio configuration HTTP handlers.
 */
export interface AudioRouteDeps {
  /** HiFi serial socket client. */
  client: HifiSerialClient;
  /** PCM router socket client. */
  pcmClient: PcmRouterClient;
  /** System status store. */
  statusStore: SystemStatusStore;
  /** Loaded service configuration. */
  config: ServiceConfig;
  /** Structured logger. */
  logger: Logger;
}

/**
 * Registers audio configuration REST handlers.
 */
export class AudioRoutes {
  /**
   * Creates route handlers.
   * @param deps - Handler dependencies.
   */
  constructor(private readonly deps: AudioRouteDeps) {}

  /**
   * Returns true when the pathname is an audio API route.
   * @param pathname - Request path.
   * @returns Match result.
   */
  matches(pathname: string): boolean {
    return pathname.startsWith("/api/audio");
  }

  /**
   * Handles an audio HTTP request.
   * @param req - Incoming request.
   * @param res - HTTP response.
   * @param pathname - Request pathname.
   * @param correlationId - Correlation id.
   * @returns True when handled.
   */
  async handle(
    req: IncomingMessage,
    res: ServerResponse,
    pathname: string,
    correlationId: string
  ): Promise<boolean> {
    if (!this.matches(pathname)) {
      return false;
    }

    try {
      if (req.method === "GET" && pathname === "/api/audio/snapshot") {
        const snapshot = await buildAudioSnapshot(
          {
            config: this.deps.config,
            hifiClient: this.deps.client,
            pcmClient: this.deps.pcmClient,
            systemStatus: this.deps.statusStore.getStatus(),
          },
          correlationId
        );
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: snapshot as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      if (req.method === "GET" && pathname === "/api/audio/airplay-source") {
        const result = await this.deps.client.getAirplaySource(correlationId);
        sendJson(res, 200, createSuccessResponse({ correlationId, data: result }));
        return true;
      }

      if (req.method === "PUT" && pathname === "/api/audio/airplay-source") {
        const body = await readBody(req);
        const parsed = JSON.parse(body) as { sourceNumber?: number };
        if (typeof parsed.sourceNumber !== "number") {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "sourceNumber is required" },
            })
          );
          return true;
        }
        const result = await this.deps.client.setAirplaySource(
          parsed.sourceNumber,
          correlationId
        );
        sendJson(res, 200, createSuccessResponse({ correlationId, data: result }));
        return true;
      }

      const zoneMatch = pathname.match(/^\/api\/audio\/zones\/(\d+)$/);
      if (req.method === "PUT" && zoneMatch) {
        const zoneNumber = Number(zoneMatch[1]);
        if (zoneNumber < 1 || zoneNumber > 16) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "zone must be 1-16" },
            })
          );
          return true;
        }

        const body = JSON.parse(await readBody(req)) as {
          controller?: ZoneControllerPatch;
          shairport?: ShairportZonePatch;
        };

        const controllerPatch = body.controller ?? {};
        const shairportPatch = body.shairport ?? {};

        if (Object.keys(controllerPatch).length > 0) {
          await this.deps.client.patchZoneController(
            zoneNumber,
            controllerPatch as Record<string, unknown>,
            correlationId
          );
        }

        for (const command of buildZoneControllerCommands(zoneNumber, controllerPatch)) {
          await this.deps.client.sendCommand(command, correlationId);
        }

        if (Object.keys(shairportPatch).length > 0) {
          await this.deps.client.updateShairportZoneSettings(
            zoneNumber,
            shairportPatch,
            correlationId
          );
        }

        const shairportRestartRequired = requiresShairportRestart(
          controllerPatch,
          shairportPatch
        );

        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: { zoneNumber, shairportRestartRequired },
          })
        );
        return true;
      }

      sendJson(
        res,
        404,
        createErrorResponse({
          correlationId,
          error: { code: "NOT_FOUND", message: `Route not found: ${pathname}` },
        })
      );
      return true;
    } catch (error) {
      const message = error instanceof Error ? error.message : "Audio request failed";
      this.deps.logger.warn({
        module: "app.backend.audio",
        event: "request_failed",
        correlationId,
        message,
      });
      sendJson(
        res,
        503,
        createErrorResponse({
          correlationId,
          error: { code: "AUDIO_UNAVAILABLE", message },
        })
      );
      return true;
    }
  }
}

/**
 * Sends a JSON API response envelope.
 * @param res - HTTP response.
 * @param status - HTTP status code.
 * @param body - Response body.
 */
function sendJson(res: ServerResponse, status: number, body: unknown): void {
  const payload = JSON.stringify(body);
  res.writeHead(status, {
    "Content-Type": "application/json",
    "Content-Length": Buffer.byteLength(payload),
  });
  res.end(payload);
}

/**
 * Reads the request body as UTF-8 text.
 * @param req - Incoming request.
 * @returns Body string.
 */
function readBody(req: IncomingMessage): Promise<string> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    req.on("data", (chunk) => chunks.push(Buffer.from(chunk)));
    req.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
    req.on("error", reject);
  });
}
