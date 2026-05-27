import type { IncomingMessage } from "node:http";
import type { Duplex } from "node:stream";
import { WebSocketServer, type WebSocket } from "ws";
import { createTransportEnvelope, encodeNdjsonLine } from "@homepi/core-transport";
import type { Logger } from "@homepi/core-logging";
import type { SystemStatusSnapshot } from "./types/system-status-types.js";

const WS_SOURCE = "homepi-backend";
const STATUS_TOPIC = "system.status";

/**
 * WebSocket shell handler for GET /ws upgrades.
 */
export class WsHandler {
  private readonly wss: WebSocketServer;

  /**
   * Creates a WebSocket handler with noServer mode.
   * @param logger - Structured logger.
   * @param getStatus - Callback returning latest system status.
   */
  constructor(
    private readonly logger: Logger,
    private readonly getStatus: () => SystemStatusSnapshot
  ) {
    this.wss = new WebSocketServer({ noServer: true });
    this.wss.on("connection", (socket, request) => {
      this.onConnection(socket, request);
    });
  }

  /**
   * Performs the HTTP upgrade handshake for WebSocket connections.
   * @param request - Incoming HTTP request.
   * @param socket - Raw TCP socket.
   * @param head - Remaining upgrade bytes.
   * @param correlationId - Request correlation identifier.
   */
  handleUpgrade(
    request: IncomingMessage,
    socket: Duplex,
    head: Buffer,
    correlationId: string
  ): void {
    const requestWithCorrelation = request as IncomingMessage & {
      homepiCorrelationId?: string;
    };
    requestWithCorrelation.homepiCorrelationId = correlationId;

    this.wss.handleUpgrade(request, socket, head, (ws) => {
      this.wss.emit("connection", ws, request);
    });
  }

  /**
   * Closes the WebSocket server.
   */
  close(): void {
    this.wss.close();
  }

  private onConnection(socket: WebSocket, request: IncomingMessage): void {
    const requestWithCorrelation = request as IncomingMessage & {
      homepiCorrelationId?: string;
    };
    const requestCorrelationId =
      requestWithCorrelation.homepiCorrelationId ??
      (typeof request.headers["x-request-id"] === "string"
        ? request.headers["x-request-id"]
        : `ws-${Date.now()}`);

    this.logger.info({
      module: "app.backend.transport",
      event: "websocket_client_connected",
      correlationId: requestCorrelationId,
      message: "WebSocket client connected",
    });

    this.sendSnapshot(socket, requestCorrelationId);

    socket.on("message", (data) => {
      this.onMessage(socket, data, requestCorrelationId);
    });

    socket.on("close", () => {
      this.logger.info({
        module: "app.backend.transport",
        event: "websocket_client_disconnected",
        correlationId: requestCorrelationId,
        message: "WebSocket client disconnected",
      });
    });

    socket.on("error", (error: Error) => {
      this.logger.warn({
        module: "app.backend.transport",
        event: "websocket_client_error",
        correlationId: requestCorrelationId,
        message: "WebSocket client error",
        data: { error: error.message },
      });
    });
  }

  private sendSnapshot(socket: WebSocket, correlationId: string): void {
    const envelope = createTransportEnvelope({
      type: "snapshot",
      source: WS_SOURCE,
      topic: STATUS_TOPIC,
      correlationId,
      payload: {
        snapshot: this.getStatus(),
      },
    });
    socket.send(encodeNdjsonLine(envelope));
  }

  private onMessage(socket: WebSocket, data: WebSocket.RawData, correlationId: string): void {
    try {
      const text = typeof data === "string" ? data : data.toString("utf8");
      const parsed = JSON.parse(text.trim()) as { type?: string; action?: string };

      const isPing =
        parsed.type === "ping" ||
        parsed.action === "ping" ||
        (parsed as { payload?: { action?: string } }).payload?.action === "ping";

      if (!isPing) {
        return;
      }

      const pong = createTransportEnvelope({
        type: "response",
        source: WS_SOURCE,
        topic: "transport.ping",
        correlationId,
        payload: { action: "pong" },
      });
      socket.send(encodeNdjsonLine(pong));
    } catch (error) {
      const message = error instanceof Error ? error.message : "Unknown parse error";
      this.logger.warn({
        module: "app.backend.transport",
        event: "websocket_message_error",
        correlationId,
        message: "Failed to handle WebSocket message",
        data: { error: message },
      });
    }
  }
}
