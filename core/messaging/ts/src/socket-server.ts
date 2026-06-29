import { createServer, type Server, type Socket } from "node:net";

import { encodeNdjsonLine, splitNdjsonLines } from "@homepi/core-transport";

import { createErrorResponse, createSuccessResponse } from "./envelope.js";
import type { MessagingError, SocketRequest, SocketResponse } from "./messaging-types.js";
import { MAX_MESSAGE_BYTES } from "./socket-client.js";

/**
 * Handler for an incoming socket command.
 */
export type CommandHandler = (
  request: SocketRequest
) => Promise<Record<string, unknown>> | Record<string, unknown>;

/**
 * Options for the NDJSON socket server.
 */
export interface SocketServerOptions {
  /** Unix socket path. */
  socketPath: string;
  /** Service name for error responses. */
  serviceName: string;
  /** Command handlers keyed by command name. */
  handlers: Record<string, CommandHandler>;
}

/**
 * Minimal NDJSON Unix socket command server.
 */
export class MessagingSocketServer {
  private server: Server | null = null;

  /**
   * @param options - Server configuration.
   */
  constructor(private readonly options: SocketServerOptions) {}

  /**
   * Starts listening on the configured socket path.
   * @returns Promise that resolves when listening.
   */
  async start(): Promise<void> {
    if (this.server) {
      return;
    }

    this.server = createServer((socket) => {
      this.handleConnection(socket);
    });

    await new Promise<void>((resolve, reject) => {
      this.server?.listen(this.options.socketPath, () => resolve());
      this.server?.once("error", reject);
    });
  }

  /**
   * Stops the server.
   */
  async stop(): Promise<void> {
    if (!this.server) {
      return;
    }

    await new Promise<void>((resolve, reject) => {
      this.server?.close((error) => {
        if (error) {
          reject(error);
          return;
        }
        resolve();
      });
    });

    this.server = null;
  }

  private handleConnection(socket: Socket): void {
    let buffer = "";

    socket.setEncoding("utf8");

    socket.on("data", (chunk: string) => {
      buffer += chunk;
      if (Buffer.byteLength(buffer, "utf8") > MAX_MESSAGE_BYTES) {
        socket.destroy();
        return;
      }

      const [lines, remainder] = splitNdjsonLines(buffer);
      buffer = remainder;

      for (const line of lines) {
        void this.handleLine(socket, line);
      }
    });
  }

  private async handleLine(socket: Socket, line: string): Promise<void> {
    let request: SocketRequest;

    try {
      request = JSON.parse(line) as SocketRequest;
    } catch {
      return;
    }

    const handler = this.options.handlers[request.command];
    if (!handler) {
      const response = createErrorResponse(request, this.unknownCommandError(request.command));
      socket.write(encodeNdjsonLine(response));
      return;
    }

    try {
      const result = await handler(request);
      const response = createSuccessResponse(request, result);
      socket.write(encodeNdjsonLine(response));
    } catch (error) {
      const response = createErrorResponse(
        request,
        this.handlerError(request, error instanceof Error ? error.message : String(error))
      );
      socket.write(encodeNdjsonLine(response));
    }
  }

  private unknownCommandError(command: string): MessagingError {
    return {
      code: "UNKNOWN_COMMAND",
      severity: "error",
      userMessage: "HomePi received an unsupported command.",
      developerMessage: `No handler registered for command ${command}`,
      service: this.options.serviceName,
      recoverable: true,
      retryable: false,
      details: { command },
    };
  }

  private handlerError(request: SocketRequest, message: string): MessagingError {
    return {
      code: "COMMAND_FAILED",
      severity: "error",
      userMessage: "HomePi could not complete the requested operation.",
      developerMessage: message,
      service: this.options.serviceName,
      recoverable: true,
      retryable: true,
      correlationId: request.correlationId,
    };
  }
}

/**
 * Writes a response line to a connected socket.
 * @param socket - Connected socket.
 * @param response - Response envelope.
 */
export function writeResponse(socket: Socket, response: SocketResponse): void {
  socket.write(encodeNdjsonLine(response));
}
