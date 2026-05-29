import { connect, type Socket } from "node:net";

import { validateEventEnvelope } from "@homepi/core-events";
import type { EventEnvelope } from "@homepi/core-events";
import type { Logger } from "@homepi/core-logging";

import type { EventBroadcaster } from "../event-broadcaster.js";

/**
 * Options for the HiFi serial SSE event bridge.
 */
export interface HifiSerialEventBridgeOptions {
  /** Unix socket path. */
  socketPath: string;
  /** Structured logger. */
  logger: Logger;
  /** SSE broadcaster. */
  broadcaster: EventBroadcaster;
  /** Called when any event is forwarded. */
  onEvent?: (timestamp: string) => void;
}

/**
 * Subscribes to native HiFi event envelopes and forwards them to SSE clients.
 */
export class HifiSerialEventBridge {
  private socket: Socket | null = null;
  private buffer = "";
  private stopped = false;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;

  /**
   * Creates an event bridge.
   * @param options - Bridge options.
   */
  constructor(private readonly options: HifiSerialEventBridgeOptions) {}

  /**
   * Starts the persistent socket subscription.
   */
  start(): void {
    this.stopped = false;
    this.connect();
  }

  /**
   * Stops the bridge and closes the socket.
   */
  stop(): void {
    this.stopped = true;
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    this.socket?.destroy();
    this.socket = null;
  }

  private connect(): void {
    if (this.stopped) {
      return;
    }

    const socket = connect(this.options.socketPath);
    this.socket = socket;

    socket.on("connect", () => {
      this.options.logger.info({
        module: "app.backend.hifi-serial",
        event: "event_bridge_connected",
        correlationId: "hifi-event-bridge",
        message: "HiFi event bridge connected",
      });
      socket.write('{"method":"subscribe","correlationId":"hifi-event-bridge"}\n');
    });

    socket.on("data", (chunk) => {
      this.buffer += chunk.toString("utf8");
      const lines = this.buffer.split("\n");
      this.buffer = lines.pop() ?? "";

      for (const line of lines) {
        if (!line.trim()) {
          continue;
        }
        this.handleLine(line);
      }
    });

    socket.on("error", (error) => {
      this.options.logger.warn({
        module: "app.backend.hifi-serial",
        event: "event_bridge_error",
        correlationId: "hifi-event-bridge",
        message: error.message,
      });
    });

    socket.on("close", () => {
      this.socket = null;
      if (!this.stopped) {
        this.scheduleReconnect();
      }
    });
  }

  private scheduleReconnect(): void {
    if (this.reconnectTimer) {
      return;
    }
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect();
    }, 5_000);
  }

  private handleLine(line: string): void {
    let parsed: unknown;
    try {
      parsed = JSON.parse(line);
    } catch {
      return;
    }

    if (
      typeof parsed !== "object" ||
      parsed === null ||
      !("event" in parsed) ||
      typeof (parsed as { event?: unknown }).event !== "string"
    ) {
      return;
    }

    const result = validateEventEnvelope(parsed);
    if (!result.valid) {
      return;
    }

    const envelope = parsed as EventEnvelope;
    this.options.broadcaster.broadcast(envelope);
    this.options.onEvent?.(envelope.timestamp);
  }
}
