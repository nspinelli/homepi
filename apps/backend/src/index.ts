import { createServer, type IncomingMessage, type ServerResponse } from "node:http";
import { loadServiceConfig } from "@homepi/core-config";
import {
  createLogger,
  createCorrelationId,
  resolveCorrelationId,
} from "@homepi/core-logging";
import {
  createSuccessResponse,
  createErrorResponse,
  getRequestCorrelationId,
} from "@homepi/core-api";
import { createHealthReport } from "@homepi/core-health";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const __dirname = dirname(fileURLToPath(import.meta.url));
const configPath = join(__dirname, "..", "config", "service-config.json");

const config = loadServiceConfig({ configPath });
const logger = createLogger({
  service: config.service,
  minLevel: config.logging.level,
});

/** Backend listen port (proxied by NGINX at homepi.local). */
const PORT = 3000;
/** Bind address for local development. */
const HOST = "127.0.0.1";

/**
 * Sends a JSON API response.
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
 * Handles HTTP requests for the backend shell.
 */
function handleRequest(req: IncomingMessage, res: ServerResponse): void {
  const correlationId = resolveCorrelationId(
    getRequestCorrelationId(req.headers)
  );
  const url = new URL(req.url ?? "/", `http://${req.headers.host ?? "localhost"}`);

  if (url.pathname === "/api/health" && req.method === "GET") {
    const report = createHealthReport({
      service: config.service,
      checks: [{ name: "http", status: "pass", message: "Backend listening" }],
    });
    sendJson(
      res,
      200,
      createSuccessResponse({
        correlationId,
        data: report as unknown as Record<string, unknown>,
      })
    );
    return;
  }

  if (url.pathname === "/api/events" && req.method === "GET") {
    res.writeHead(200, {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
      "X-Request-ID": correlationId,
    });
    res.write(`event: ready\ndata: ${JSON.stringify({ ok: true })}\n\n`);
    return;
  }

  if (url.pathname === "/api/ws" && req.method === "GET") {
    sendJson(
      res,
      426,
      createErrorResponse({
        correlationId,
        error: {
          code: "UPGRADE_REQUIRED",
          message: "WebSocket upgrade required; use a WebSocket client",
        },
      })
    );
    return;
  }

  sendJson(
    res,
    404,
    createErrorResponse({
      correlationId,
      error: {
        code: "NOT_FOUND",
        message: `Route not found: ${url.pathname}`,
      },
    })
  );
}

const server = createServer(handleRequest);

server.listen(PORT, HOST, () => {
  logger.info({
    module: "app.backend",
    event: "service_started",
    correlationId: createCorrelationId("startup"),
    message: "HomePi backend started",
    data: { host: HOST, port: PORT, environment: config.environment },
  });
});
