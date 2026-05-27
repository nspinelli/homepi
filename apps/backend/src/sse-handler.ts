import type { IncomingMessage, ServerResponse } from "node:http";
import type { Logger } from "@homepi/core-logging";
import { EventBroadcaster } from "./event-broadcaster.js";

/**
 * Handles Server-Sent Events connections at GET /events.
 */
export class SseHandler {
  /**
   * Creates an SSE handler.
   * @param logger - Structured logger.
   * @param broadcaster - Event broadcaster instance.
   */
  constructor(
    private readonly logger: Logger,
    private readonly broadcaster: EventBroadcaster
  ) {}

  /**
   * Upgrades an HTTP request to an SSE stream.
   * @param req - Incoming HTTP request.
   * @param res - HTTP response.
   * @param correlationId - Request correlation identifier.
   */
  handle(req: IncomingMessage, res: ServerResponse, correlationId: string): void {
    res.writeHead(200, {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
      "X-Request-ID": correlationId,
    });

    this.broadcaster.addSubscriber(res, correlationId);

    req.on("close", () => {
      this.broadcaster.removeSubscriber(res, correlationId);
    });

    req.on("error", (error: Error) => {
      this.logger.warn({
        module: "app.backend.events",
        event: "sse_client_error",
        correlationId,
        message: "SSE client error",
        data: { error: error.message },
      });
      this.broadcaster.removeSubscriber(res, correlationId);
    });
  }
}
