import { connect, type Socket } from "node:net";

import { validateEventEnvelope } from "@homepi/core-events";
import type { EventEnvelope } from "@homepi/core-events";
import type { Logger } from "@homepi/core-logging";

import type { EventBroadcaster } from "../event-broadcaster.js";
import { EventBridgeReconnect } from "../status/event-bridge-reconnect.js";
import { mapEnvelopeToStatusPatch } from "../status/service-event-handlers.js";
import type { StatusUpdateCoordinator } from "../status/status-update-coordinator.js";
import {
  adaptBrokerEnvelopeForUi,
  BROKER_AUDIO_TOPICS,
  shouldDropBrokerEnvelope,
} from "../audio/audio-ui-bridge.js";
import type { AudioBrokerSnapshotStore } from "../audio/audio-broker-snapshot-store.js";

/**
 * Options for the central core/events SSE bridge.
 */
export interface EventsBrokerBridgeOptions {
  /** Unix socket path for /run/homepi/events.sock. */
  socketPath: string;
  /** Backend source name registered with the broker. */
  source?: string;
  /** Structured logger. */
  logger: Logger;
  /** SSE broadcaster. */
  broadcaster: EventBroadcaster;
  /** Status update coordinator. */
  coordinator: StatusUpdateCoordinator;
  /** Topic patterns to subscribe to. */
  topics?: string[];
  /** Optional broker snapshot cache for REST hydration. */
  snapshotStore?: AudioBrokerSnapshotStore;
  /** Called when connection state changes. */
  onConnectionChange?: (connected: boolean) => void;
}

const DEFAULT_TOPICS = [...BROKER_AUDIO_TOPICS];

/**
 * Subscribes to the HomePi core/events broker and forwards envelopes to SSE clients.
 */
export class EventsBrokerBridge {
  private socket: Socket | null = null;
  private buffer = "";
  private stopped = false;
  private connected = false;
  private readonly reconnect: EventBridgeReconnect;

  /**
   * Creates an events broker bridge.
   * @param options - Bridge options.
   */
  constructor(private readonly options: EventsBrokerBridgeOptions) {
    this.reconnect = new EventBridgeReconnect({
      logger: options.logger,
      module: "app.backend.events",
      correlationId: "events-broker-bridge",
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
   * Starts the persistent broker subscription.
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
    const source = this.options.source ?? "homepi-backend";
    const topics = this.options.topics ?? DEFAULT_TOPICS;

    socket.on("connect", () => {
      this.reconnect.resetBackoff();
      this.setConnected(true);
      this.options.logger.info({
        module: "app.backend.events",
        event: "event_bridge_connected",
        correlationId: "events-broker-bridge",
        message: "Core events broker bridge connected",
      });
      socket.write(
        `${JSON.stringify({
          method: "register",
          source,
          subscribes: topics,
          publishes: ["modules.zone.command"],
        })}\n`
      );
      socket.write(
        `${JSON.stringify({
          method: "subscribe",
          topics,
        })}\n`
      );
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

    socket.on("error", (error: NodeJS.ErrnoException) => {
      const missingSocket = error.code === "ENOENT";
      this.options.logger[missingSocket ? "debug" : "warn"]({
        module: "app.backend.events",
        event: missingSocket ? "event_bridge_waiting" : "event_bridge_error",
        correlationId: "events-broker-bridge",
        message: missingSocket
          ? `Waiting for ${this.options.socketPath}`
          : error.message,
      });
    });

    socket.on("close", () => {
      this.socket = null;
      this.setConnected(false);
      if (!this.stopped) {
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

    const envelope = this.adaptEnvelope(parsed as EventEnvelope);
    if (shouldDropBrokerEnvelope(envelope)) {
      return;
    }

    this.options.snapshotStore?.ingest(parsed as EventEnvelope);

    const result = validateEventEnvelope(envelope);
    if (!result.valid) {
      return;
    }

    this.options.broadcaster.broadcast(envelope);

    const patch = mapEnvelopeToStatusPatch(envelope);
    if (patch) {
      this.options.coordinator.patchAndBroadcast(
        patch,
        "events-broker-bridge",
        envelope.timestamp
      );
    } else if (envelope.timestamp) {
      this.options.coordinator.patchAndBroadcast({}, "events-broker-bridge", envelope.timestamp);
    }
  }

  /**
   * Maps new broker event names to legacy SSE names the frontend already handles.
   * @param envelope - Raw broker envelope.
   * @returns Adapted envelope for UI consumers.
   */
  private adaptEnvelope(envelope: EventEnvelope): EventEnvelope {
    return adaptBrokerEnvelopeForUi(envelope);
  }
}
