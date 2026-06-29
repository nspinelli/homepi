#!/usr/bin/env node
import { mkdirSync, rmSync } from "node:fs";
import { dirname } from "node:path";

import { createLogger, createCorrelationId } from "@homepi/core-logging";
import {
  createErrorResponse,
  createSuccessResponse,
  type BrokerEvent,
  type SocketRequest,
} from "@homepi/core-messaging";
import { encodeNdjsonLine, splitNdjsonLines } from "@homepi/core-transport";
import { createServer } from "node:net";

import { BrokerState } from "./broker-state.js";

const SOCKET_PATH = process.env.HOMEPI_BROKER_SOCKET ?? "/run/homepi/broker/broker.sock";
const logger = createLogger({ service: "homepi-broker", minLevel: "INFO" });
const brokerState = new BrokerState();

/**
 * Starts the HomePi event broker Unix socket server.
 */
async function main(): Promise<void> {
  mkdirSync(dirname(SOCKET_PATH), { recursive: true });
  try {
    rmSync(SOCKET_PATH, { force: true });
  } catch {
    /* ignore */
  }

  const server = createServer((socket) => {
    let buffer = "";
    const subscriptions: string[] = [];
    let unsubscribe: (() => void) | null = null;

    const detach = (): void => {
      unsubscribe?.();
      unsubscribe = null;
    };

    const listener = (event: BrokerEvent): void => {
      if (socket.destroyed || !socket.writable) {
        detach();
        return;
      }

      socket.write(encodeNdjsonLine({ type: "event", event }), (error) => {
        if (error) {
          detach();
          socket.destroy();
        }
      });
    };

    socket.setEncoding("utf8");

    socket.on("error", () => {
      detach();
    });

    socket.on("data", (chunk: string) => {
      buffer += chunk;
      const [lines, remainder] = splitNdjsonLines(buffer);
      buffer = remainder;

      for (const line of lines) {
        void handleLine(socket, line, subscriptions, (topics) => {
          unsubscribe?.();
          unsubscribe = brokerState.subscribe(topics, listener);
        });
      }
    });

    socket.on("close", () => {
      detach();
    });
  });

  await new Promise<void>((resolve, reject) => {
    server.listen(SOCKET_PATH, () => resolve());
    server.once("error", reject);
  });

  logger.info({
    module: "broker",
    event: "service_started",
    correlationId: createCorrelationId("startup"),
    message: "homepi-broker listening",
    data: { socketPath: SOCKET_PATH },
  });

  const shutdown = (): void => {
    server.close();
    rmSync(SOCKET_PATH, { force: true });
    process.exit(0);
  };

  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);
}

/**
 * Handles one NDJSON command line from a broker client.
 * @param socket - Client socket.
 * @param line - Raw NDJSON line.
 * @param subscriptions - Mutable subscription list for this client.
 * @param listener - Event listener for subscriptions.
 */
async function handleLine(
  socket: import("node:net").Socket,
  line: string,
  subscriptions: string[],
  onSubscribe: (topics: string[]) => void
): Promise<void> {
  let request: SocketRequest;

  try {
    request = JSON.parse(line) as SocketRequest;
  } catch {
    return;
  }

  if (request.command === "subscribe") {
    const topics = (request.payload?.topics as string[] | undefined) ?? [];
    subscriptions.push(...topics);
    onSubscribe(topics);
    socket.write(
      encodeNdjsonLine(createSuccessResponse(request, { subscribed: true, topics }))
    );
    return;
  }

  try {
    const result = brokerState.handleCommand(request);
    socket.write(encodeNdjsonLine(createSuccessResponse(request, result)));
  } catch (error) {
    socket.write(
      encodeNdjsonLine(
        createErrorResponse(request, {
          code: "BROKER_COMMAND_FAILED",
          severity: "error",
          userMessage: "Live update broker could not complete the request.",
          developerMessage: error instanceof Error ? error.message : String(error),
          service: "homepi-broker",
          recoverable: true,
          retryable: true,
          correlationId: request.correlationId,
        })
      )
    );
  }
}

main().catch((error: unknown) => {
  logger.error({
    module: "broker",
    event: "service_failed",
    correlationId: createCorrelationId("fatal"),
    message: error instanceof Error ? error.message : String(error),
  });
  process.exit(1);
});
