import { connect, type Socket } from "node:net";

import { validateEventEnvelope } from "@homepi/core-events";
import type { EventEnvelope } from "@homepi/core-events";
import type { Logger } from "@homepi/core-logging";

import type { EventBroadcaster } from "../event-broadcaster.js";
import { EventBridgeReconnect } from "../status/event-bridge-reconnect.js";
import { mapEnvelopeToStatusPatch } from "../status/service-event-handlers.js";
import type { StatusUpdateCoordinator } from "../status/status-update-coordinator.js";

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
  /** Status update coordinator. */
  coordinator: StatusUpdateCoordinator;
  /** Called when connection state changes. */
  onConnectionChange?: (connected: boolean) => void;
}

/**
 * Subscribes to native HiFi event envelopes and forwards them to SSE clients.
 */
export class HifiSerialEventBridge {
  private socket: Socket | null = null;
  private buffer = "";
  private stopped = false;
  private connected = false;
  private readonly reconnect: EventBridgeReconnect;

  /**
   * Creates an event bridge.
   * @param options - Bridge options.
   */
  constructor(private readonly options: HifiSerialEventBridgeOptions) {
    this.reconnect = new EventBridgeReconnect({
      logger: options.logger,
      module: "app.backend.hifi-serial",
      correlationId: "hifi-event-bridge",
      connect: () => this.connect(),
      isStopped: () => this.stopped,
    });
  }

  /**
   * Returns whether the bridge is currently connected.
   * @returns True when socket is connected.
   */
  isConnected(): boolean {
    return this.connected;
  }

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
    this.reconnect.clearTimer();
    this.setConnected(false);
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
      this.reconnect.resetBackoff();
      this.setConnected(true);
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
      this.setConnected(false);
      if (!this.stopped) {
        this.options.coordinator.markServiceOffline("hifiSerial", "hifi-event-bridge");
        this.reconnect.scheduleReconnect();
      }
    });
  }

  private setConnected(connected: boolean): void {
    if (this.connected === connected) {
      return;
    }
    this.connected = connected;
    this.options.onConnectionChange?.(connected);
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

    const patch = mapEnvelopeToStatusPatch(envelope);
    if (patch) {
      this.options.coordinator.patchAndBroadcast(
        patch,
        "hifi-event-bridge",
        envelope.timestamp
      );
    } else if (envelope.timestamp) {
      this.options.coordinator.patchAndBroadcast(
        {},
        "hifi-event-bridge",
        envelope.timestamp
      );
    }
  }
}
