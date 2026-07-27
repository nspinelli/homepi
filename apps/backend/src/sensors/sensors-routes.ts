import type { IncomingMessage, ServerResponse } from "node:http";

import type { Logger } from "@homepi/core-logging";
import { createErrorResponse, createSuccessResponse } from "@homepi/core-api";

import type { HealthClient } from "../health/health-client.js";
import { buildSensorsSnapshot } from "./build-sensors-snapshot.js";
import type { SensorsClient } from "./sensors-client.js";

/**
 * Dependencies for contact sensors HTTP handlers.
 */
export interface SensorsRouteDeps {
  /** Sensors facade client. */
  client: SensorsClient;
  /** Health observer client. */
  healthClient: HealthClient;
  /** Structured logger. */
  logger: Logger;
}

/**
 * Registers contact sensors REST handlers.
 */
export class SensorsRoutes {
  /**
   * Creates route handlers.
   * @param deps - Handler dependencies.
   */
  constructor(private readonly deps: SensorsRouteDeps) {}

  /**
   * Returns true when the pathname is a contact sensors API route.
   * @param pathname - Request path.
   * @returns Match result.
   */
  matches(pathname: string): boolean {
    return pathname.startsWith("/api/contact-sensors");
  }

  /**
   * Handles a contact sensors HTTP request.
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
      if (req.method === "GET" && pathname === "/api/contact-sensors") {
        const snapshot = await buildSensorsSnapshot(
          {
            sensorsClient: this.deps.client,
            healthClient: this.deps.healthClient,
          },
          correlationId
        );
        sendJson(
          res,
          200,
          createSuccessResponse({ correlationId, data: snapshot as unknown as Record<string, unknown> })
        );
        return true;
      }

      if (req.method === "GET" && pathname === "/api/contact-sensors/diagnostics") {
        const result = await this.deps.client.send(
          "sensors.diagnostics.get",
          {},
          correlationId
        );
        sendJson(res, 200, createSuccessResponse({ correlationId, data: result }));
        return true;
      }

      const sensorMatch = pathname.match(/^\/api\/contact-sensors\/([^/]+)$/);
      if (sensorMatch) {
        const sensorId = decodeURIComponent(sensorMatch[1] ?? "");

        if (req.method === "GET") {
          const result = await this.deps.client.send(
            "sensors.sensor.get",
            { sensorId },
            correlationId
          );
          sendJson(res, 200, createSuccessResponse({ correlationId, data: result }));
          return true;
        }

        if (req.method === "PATCH") {
          const body = await readJsonBody(req);
          const result = await this.deps.client.send(
            "sensors.sensor.patch",
            { sensorId, patch: body },
            correlationId
          );
          sendJson(res, 200, createSuccessResponse({ correlationId, data: result }));
          return true;
        }
      }

      sendJson(
        res,
        404,
        createErrorResponse({
          correlationId,
          error: { code: "NOT_FOUND", message: "Route not found" },
        })
      );
      return true;
    } catch (error) {
      this.deps.logger.error({
        module: "app.backend.sensors",
        event: "route_error",
        correlationId,
        message: error instanceof Error ? error.message : String(error),
      });
      sendJson(
        res,
        500,
        createErrorResponse({
          correlationId,
          error: {
            code: "INTERNAL_ERROR",
            message:
              error instanceof Error ? error.message : "Contact sensors request failed",
          },
        })
      );
      return true;
    }
  }
}

/**
 * Sends a JSON HTTP response.
 * @param res - HTTP response.
 * @param status - Status code.
 * @param body - Response body.
 */
function sendJson(res: ServerResponse, status: number, body: unknown): void {
  res.writeHead(status, { "Content-Type": "application/json" });
  res.end(JSON.stringify(body));
}

/**
 * Reads JSON body from a request.
 * @param req - Incoming request.
 * @returns Parsed JSON object.
 */
async function readJsonBody(req: IncomingMessage): Promise<Record<string, unknown>> {
  const chunks: Buffer[] = [];
  for await (const chunk of req) {
    chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
  }
  const text = Buffer.concat(chunks).toString("utf8").trim();
  if (!text) {
    return {};
  }
  return JSON.parse(text) as Record<string, unknown>;
}
