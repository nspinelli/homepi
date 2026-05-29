import type { IncomingMessage, ServerResponse } from "node:http";

import type { Logger } from "@homepi/core-logging";
import { createErrorResponse, createSuccessResponse } from "@homepi/core-api";

import type { HifiSerialClient } from "./hifi-serial-client.js";

/**
 * Dependencies for HiFi serial HTTP handlers.
 */
export interface HifiSerialRouteDeps {
  /** Socket client. */
  client: HifiSerialClient;
  /** Structured logger. */
  logger: Logger;
}

/**
 * Registers HiFi serial REST handlers.
 */
export class HifiSerialRoutes {
  /**
   * Creates route handlers.
   * @param deps - Handler dependencies.
   */
  constructor(private readonly deps: HifiSerialRouteDeps) {}

  /**
   * Returns true when the pathname is a HiFi serial API route.
   * @param pathname - Request path.
   * @returns Match result.
   */
  matches(pathname: string): boolean {
    return pathname.startsWith("/api/hifi-serial");
  }

  /**
   * Handles a HiFi serial HTTP request.
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
      if (req.method === "GET" && pathname === "/api/hifi-serial/health") {
        const health = await this.deps.client.getHealth(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: health as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      if (req.method === "POST" && pathname === "/api/hifi-serial/sync") {
        const result = await this.deps.client.syncController(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: result as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      if (req.method === "GET" && pathname === "/api/hifi-serial/snapshot") {
        const snapshot = await this.deps.client.getSnapshot(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({ correlationId, data: snapshot })
        );
        return true;
      }

      if (req.method === "GET" && pathname === "/api/hifi-serial/controller") {
        const controller = await this.deps.client.getController(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: controller,
          })
        );
        return true;
      }

      if (req.method === "GET" && pathname === "/api/hifi-serial/zones") {
        const zones = await this.deps.client.getZones(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: zones as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      const zoneMatch = pathname.match(/^\/api\/hifi-serial\/zones\/(\d+)$/);
      if (req.method === "GET" && zoneMatch) {
        const zoneNumber = Number(zoneMatch[1]);
        const { zones } = await this.deps.client.getZones(correlationId);
        const zone = zones.find(
          (item) =>
            typeof item === "object" &&
            item !== null &&
            (item as { zoneNumber?: number }).zoneNumber === zoneNumber
        );
        if (!zone) {
          sendJson(
            res,
            404,
            createErrorResponse({
              correlationId,
              error: { code: "NOT_FOUND", message: `Zone ${zoneNumber} not found` },
            })
          );
          return true;
        }
        sendJson(res, 200, createSuccessResponse({ correlationId, data: { zone } }));
        return true;
      }

      if (req.method === "GET" && pathname === "/api/hifi-serial/sources") {
        const sources = await this.deps.client.getSources(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: sources as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      const sourceMatch = pathname.match(/^\/api\/hifi-serial\/sources\/(\d+)$/);
      if (req.method === "GET" && sourceMatch) {
        const sourceNumber = Number(sourceMatch[1]);
        const { sources } = await this.deps.client.getSources(correlationId);
        const source = sources.find(
          (item) =>
            typeof item === "object" &&
            item !== null &&
            (item as { sourceNumber?: number }).sourceNumber === sourceNumber
        );
        if (!source) {
          sendJson(
            res,
            404,
            createErrorResponse({
              correlationId,
              error: { code: "NOT_FOUND", message: `Source ${sourceNumber} not found` },
            })
          );
          return true;
        }
        sendJson(res, 200, createSuccessResponse({ correlationId, data: { source } }));
        return true;
      }

      if (req.method === "GET" && pathname === "/api/hifi-serial/groups") {
        const groups = await this.deps.client.getGroups(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: groups as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      const groupMatch = pathname.match(/^\/api\/hifi-serial\/groups\/(\d+)$/);
      if (req.method === "GET" && groupMatch) {
        const groupNumber = Number(groupMatch[1]);
        const { groups } = await this.deps.client.getGroups(correlationId);
        const group = groups.find(
          (item) =>
            typeof item === "object" &&
            item !== null &&
            (item as { groupNumber?: number }).groupNumber === groupNumber
        );
        if (!group) {
          sendJson(
            res,
            404,
            createErrorResponse({
              correlationId,
              error: { code: "NOT_FOUND", message: `Group ${groupNumber} not found` },
            })
          );
          return true;
        }
        sendJson(res, 200, createSuccessResponse({ correlationId, data: { group } }));
        return true;
      }

      if (req.method === "GET" && pathname === "/api/hifi-serial/language-strings") {
        const strings = await this.deps.client.getLanguageStrings(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: strings as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      if (req.method === "POST" && pathname === "/api/hifi-serial/commands") {
        const body = await readBody(req);
        const parsed = JSON.parse(body) as { command?: string };
        if (!parsed.command) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "command is required" },
            })
          );
          return true;
        }
        const result = await this.deps.client.sendCommand(parsed.command, correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: result as unknown as Record<string, unknown>,
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
      const message = error instanceof Error ? error.message : "HiFi serial request failed";
      this.deps.logger.warn({
        module: "app.backend.hifi-serial",
        event: "request_failed",
        correlationId,
        message,
      });
      sendJson(
        res,
        503,
        createErrorResponse({
          correlationId,
          error: { code: "HIFI_SERIAL_UNAVAILABLE", message },
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
