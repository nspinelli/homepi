import { connect, type Socket } from "node:net";

import { encodeNdjsonLine, splitNdjsonLines } from "@homepi/core-transport";

import { createRequest, parseLegacyResponseLine, parseResponseLine } from "./envelope.js";
import type { LegacyRpcRequest, SocketRequest, SocketResponse } from "./messaging-types.js";

/** Default socket RPC timeout in milliseconds. */
export const DEFAULT_SOCKET_TIMEOUT_MS = 10_000;

/** Maximum inbound message size in bytes. */
export const MAX_MESSAGE_BYTES = 256_000;

/**
 * Options for a one-shot socket RPC call.
 */
export interface SocketRpcOptions {
  /** Unix socket path. */
  socketPath: string;
  /** v1 request envelope. */
  request: SocketRequest;
  /** Timeout in milliseconds. */
  timeoutMs?: number;
}

/**
 * Options for a legacy native RPC call.
 */
export interface LegacyRpcOptions {
  /** Unix socket path. */
  socketPath: string;
  /** Legacy request body. */
  request: LegacyRpcRequest;
  /** Timeout in milliseconds. */
  timeoutMs?: number;
}

/**
 * Sends a v1 request and waits for the matching response line.
 * @param options - RPC options.
 * @returns Socket response envelope.
 */
export function socketRpc(options: SocketRpcOptions): Promise<SocketResponse> {
  const timeoutMs = options.timeoutMs ?? DEFAULT_SOCKET_TIMEOUT_MS;

  return new Promise((resolve, reject) => {
    const socket = connect(options.socketPath);
    let buffer = "";
    let settled = false;

    const finish = (handler: () => void): void => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timer);
      socket.destroy();
      handler();
    };

    const timer = setTimeout(() => {
      finish(() => {
        reject(new Error(`Socket RPC timed out after ${timeoutMs}ms`));
      });
    }, timeoutMs);

    socket.setEncoding("utf8");

    socket.on("error", (error) => {
      finish(() => {
        reject(error);
      });
    });

    socket.on("connect", () => {
      socket.write(encodeNdjsonLine(options.request));
    });

    socket.on("data", (chunk: string) => {
      buffer += chunk;
      if (Buffer.byteLength(buffer, "utf8") > MAX_MESSAGE_BYTES) {
        finish(() => {
          reject(new Error("Socket RPC response exceeded max message size"));
        });
        return;
      }

      const [lines, remainder] = splitNdjsonLines(buffer);
      buffer = remainder;

      for (const line of lines) {
        const response = parseResponseLine(line);
        if (response && response.id === options.request.id) {
          finish(() => {
            resolve(response);
          });
          return;
        }
      }
    });

    socket.on("close", () => {
      finish(() => {
        reject(new Error("Socket closed before response"));
      });
    });
  });
}

/**
 * Sends a legacy `{method}` RPC and waits for the first `{ok}` response line.
 * @param options - Legacy RPC options.
 * @returns Parsed legacy response object.
 */
export function legacyRpc(options: LegacyRpcOptions): Promise<Record<string, unknown>> {
  const timeoutMs = options.timeoutMs ?? DEFAULT_SOCKET_TIMEOUT_MS;

  return new Promise((resolve, reject) => {
    const socket = connect(options.socketPath);
    let buffer = "";
    let settled = false;

    const finish = (handler: () => void): void => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timer);
      socket.destroy();
      handler();
    };

    const timer = setTimeout(() => {
      finish(() => {
        reject(new Error(`Legacy RPC timed out after ${timeoutMs}ms`));
      });
    }, timeoutMs);

    socket.setEncoding("utf8");

    socket.on("error", (error) => {
      finish(() => reject(error));
    });

    socket.on("connect", () => {
      socket.write(encodeNdjsonLine(options.request));
    });

    socket.on("data", (chunk: string) => {
      buffer += chunk;
      const [lines, remainder] = splitNdjsonLines(buffer);
      buffer = remainder;

      for (const line of lines) {
        const parsed = parseLegacyResponseLine(line);
        if (!parsed || parsed.event !== undefined) {
          continue;
        }
        if (typeof parsed.ok === "boolean") {
          finish(() => resolve(parsed));
          return;
        }
      }
    });

    socket.on("close", () => {
      finish(() => reject(new Error("Socket closed before legacy response")));
    });
  });
}

/**
 * Sends a v1 command using shorthand fields.
 * @param socketPath - Unix socket path.
 * @param source - Calling service.
 * @param target - Target service.
 * @param command - Command name.
 * @param payload - Optional payload.
 * @param timeoutMs - Optional timeout.
 * @returns Socket response.
 */
export function sendCommand(
  socketPath: string,
  source: string,
  target: string,
  command: string,
  payload?: Record<string, unknown>,
  timeoutMs?: number
): Promise<SocketResponse> {
  const request = createRequest({ source, target, command, payload });
  return socketRpc({ socketPath, request, timeoutMs });
}

/**
 * Checks whether a Unix socket path exists by attempting connection.
 * @param socketPath - Socket path.
 * @param timeoutMs - Connection timeout.
 * @returns True when connect succeeds.
 */
export function isSocketReachable(
  socketPath: string,
  timeoutMs = 2_000
): Promise<boolean> {
  return new Promise((resolve) => {
    const socket: Socket = connect(socketPath);
    const timer = setTimeout(() => {
      socket.destroy();
      resolve(false);
    }, timeoutMs);

    socket.on("connect", () => {
      clearTimeout(timer);
      socket.destroy();
      resolve(true);
    });

    socket.on("error", () => {
      clearTimeout(timer);
      resolve(false);
    });
  });
}
