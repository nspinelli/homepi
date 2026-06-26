import type { IncomingMessage, ServerResponse } from "node:http";

import { createErrorResponse, createSuccessResponse } from "@homepi/core-api";
import type { Logger } from "@homepi/core-logging";

import {
  derivePagingApiKeyPrefix,
  hashPagingApiKey,
  PagingApiKeyValidationError,
} from "./paging-api-key.js";
import type { PagingClient } from "./paging-client.js";

/**
 * Dependencies for paging API key settings routes.
 */
export interface PagingApiKeyRouteDeps {
  /** Paging service Unix socket client. */
  client: PagingClient;
  /** Structured logger. */
  logger: Logger;
}

/**
 * Registers paging API key settings handlers under /api/audio/settings.
 */
export class PagingApiKeyRoutes {
  /**
   * Creates route handlers.
   * @param deps - Handler dependencies.
   */
  constructor(private readonly deps: PagingApiKeyRouteDeps) {}

  /**
   * Returns true when the pathname is a paging API key settings route.
   * @param pathname - Request path.
   * @returns True when this class should handle the request.
   */
  matches(pathname: string): boolean {
    return (
      pathname === "/api/audio/settings" ||
      pathname === "/api/audio/settings/paging-api-key"
    );
  }

  /**
   * Handles an audio settings request for paging API key operations.
   * @param req - Incoming request.
   * @param res - HTTP response.
   * @param pathname - Request pathname.
   * @param correlationId - Request correlation id.
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
      if (req.method === "GET" && pathname === "/api/audio/settings") {
        const apiKey = await this.deps.client.getApiKey(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: {
              pagingApiKeyConfigured: apiKey.configured,
              pagingApiKeyPrefix: apiKey.prefix,
            },
          })
        );
        return true;
      }

      if (req.method === "PUT" && pathname === "/api/audio/settings/paging-api-key") {
        const body = JSON.parse(await readBody(req)) as { apiKey?: string };
        if (!body.apiKey || typeof body.apiKey !== "string") {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "apiKey is required" },
            })
          );
          return true;
        }

        const prefix = derivePagingApiKeyPrefix(body.apiKey);
        const apiKeyHash = await hashPagingApiKey(body.apiKey);
        await this.deps.client.setApiKey(apiKeyHash, prefix, correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: {
              ok: true,
              pagingApiKeyConfigured: true,
              pagingApiKeyPrefix: prefix,
            },
          })
        );
        return true;
      }

      if (req.method === "DELETE" && pathname === "/api/audio/settings/paging-api-key") {
        await this.deps.client.clearApiKey(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: {
              ok: true,
              pagingApiKeyConfigured: false,
              pagingApiKeyPrefix: null,
            },
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
      if (error instanceof PagingApiKeyValidationError) {
        sendJson(
          res,
          400,
          createErrorResponse({
            correlationId,
            error: { code: "INVALID_REQUEST", message: error.message },
          })
        );
        return true;
      }

      const message = error instanceof Error ? error.message : "Paging settings request failed";
      this.deps.logger.warn({
        module: "app.backend.paging-settings",
        event: "request_failed",
        correlationId,
        message,
      });
      sendJson(
        res,
        503,
        createErrorResponse({
          correlationId,
          error: { code: "PAGING_SETTINGS_UNAVAILABLE", message },
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
