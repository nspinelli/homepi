import type { IncomingMessage, ServerResponse } from "node:http";

import type { Logger } from "@homepi/core-logging";
import { createErrorResponse, createSuccessResponse } from "@homepi/core-api";

import type { UsbAssignments } from "./usb-devices-types.js";
import type { UsbDevicesClient } from "./usb-devices-client.js";

/**
 * Dependencies for USB devices HTTP handlers.
 */
export interface UsbDevicesRouteDeps {
  /** Socket client. */
  client: UsbDevicesClient;
  /** Structured logger. */
  logger: Logger;
}

/**
 * Registers USB devices REST handlers on the HTTP server router.
 */
export class UsbDevicesRoutes {
  /**
   * Creates route handlers.
   * @param deps - Handler dependencies.
   */
  constructor(private readonly deps: UsbDevicesRouteDeps) {}

  /**
   * Returns true when the pathname is a USB devices API route.
   * @param pathname - Request path.
   * @returns Match result.
   */
  matches(pathname: string): boolean {
    return pathname.startsWith("/api/usb-devices");
  }

  /**
   * Handles a USB devices HTTP request.
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
      if (req.method === "GET" && pathname === "/api/usb-devices") {
        const devices = await this.deps.client.listDevices(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({ correlationId, data: { devices } as Record<string, unknown> })
        );
        return true;
      }

      if (req.method === "GET" && pathname === "/api/usb-devices/assignments") {
        const assignments = await this.deps.client.getAssignments(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: assignments as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      const capabilitiesMatch = pathname.match(/^\/api\/usb-devices\/([^/]+)\/audio-capabilities$/);
      if (req.method === "GET" && capabilitiesMatch) {
        const deviceId = decodeURIComponent(capabilitiesMatch[1] ?? "");
        const capabilities = await this.deps.client.getAudioCapabilities(deviceId, correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({ correlationId, data: capabilities as unknown as Record<string, unknown> })
        );
        return true;
      }

      if (req.method === "GET" && pathname === "/api/usb-devices/operating-profile") {
        const profile = await this.deps.client.getOperatingProfile(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({ correlationId, data: profile })
        );
        return true;
      }

      if (req.method === "GET" && pathname === "/api/usb-devices/health") {
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

      if (req.method === "PUT" && pathname === "/api/usb-devices/assignments") {
        const body = await readBody(req);
        const assignments = parseAssignments(body);
        const saved = await this.deps.client.setAssignments(assignments, correlationId);
        this.deps.logger.info({
          module: "app.backend.usb-devices",
          event: "assignments_saved",
          correlationId,
          message: "USB role assignments saved",
          data: { assignments: saved },
        });
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: saved as unknown as Record<string, unknown>,
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
      const message = error instanceof Error ? error.message : "USB devices request failed";
      this.deps.logger.warn({
        module: "app.backend.usb-devices",
        event: "usb_devices_request_failed",
        correlationId,
        message,
      });
      sendJson(
        res,
        503,
        createErrorResponse({
          correlationId,
          error: { code: "USB_DEVICES_UNAVAILABLE", message },
        })
      );
      return true;
    }
  }
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

/**
 * Parses assignments JSON from a PUT body.
 * @param body - Raw JSON body.
 * @returns Parsed assignments.
 */
function parseAssignments(body: string): UsbAssignments {
  const parsed = JSON.parse(body || "{}") as Partial<UsbAssignments>;
  return {
    serial: parsed.serial ?? null,
    audioPrimary: parsed.audioPrimary ?? null,
    paging: parsed.paging ?? null,
    audioPrimaryProfile: parsed.audioPrimaryProfile ?? null,
  };
}

/**
 * Sends a JSON HTTP response.
 * @param res - Response object.
 * @param status - HTTP status.
 * @param body - Response envelope.
 */
function sendJson(res: ServerResponse, status: number, body: unknown): void {
  const payload = JSON.stringify(body);
  res.writeHead(status, {
    "Content-Type": "application/json",
    "Content-Length": Buffer.byteLength(payload),
  });
  res.end(payload);
}
