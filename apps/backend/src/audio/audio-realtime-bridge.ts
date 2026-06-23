import { connect, type Socket } from "node:net";

import { createEventEnvelope } from "@homepi/core-events";
import type { Logger } from "@homepi/core-logging";

import type { EventBroadcaster } from "../event-broadcaster.js";
import { EventBridgeReconnect } from "../status/event-bridge-reconnect.js";
import type { StatusUpdateCoordinator } from "../status/status-update-coordinator.js";

/** Latest progress frame from audio-realtime.sock. */
export interface AudioRealtimeFrame {
  ownerZoneId: number;
  trackId: string;
  playing: boolean;
  positionMs: number;
  durationMs: number;
  progressSource: string;
  wallTime?: string;
  monotonicMs?: number;
}

/**
 * Options for the audio realtime progress bridge.
 */
export interface AudioRealtimeBridgeOptions {
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
 * Subscribes to homepi-metadata audio-realtime.sock and forwards progress to SSE clients.
 */
export class AudioRealtimeBridge {
  private socket: Socket | null = null;
  private buffer = "";
  private stopped = false;
  private connected = false;
  private latestFrame: AudioRealtimeFrame | null = null;
  private readonly reconnect: EventBridgeReconnect;

  /**
   * Creates an audio realtime bridge.
   * @param options - Bridge options.
   */
  constructor(private readonly options: AudioRealtimeBridgeOptions) {
    this.reconnect = new EventBridgeReconnect({
      logger: options.logger,
      module: "app.backend.audio-realtime",
      correlationId: "audio-realtime-bridge",
      connect: () => this.connect(),
      isStopped: () => this.stopped,
    });
  }

  /**
   * Returns the latest progress frame received from the realtime socket.
   * @returns Latest frame or null when unavailable.
   */
  getLatestFrame(): AudioRealtimeFrame | null {
    return this.latestFrame;
  }

  /**
   * Returns whether the bridge is currently connected.
   * @returns True when socket is connected.
   */
  isConnected(): boolean {
    return this.connected;
  }

  /** Starts the persistent socket subscription. */
  start(): void {
    this.stopped = false;
    this.connect();
  }

  /** Stops the bridge and closes the socket. */
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
        module: "app.backend.audio-realtime",
        event: "realtime_bridge_connected",
        correlationId: "audio-realtime-bridge",
        message: "Audio realtime bridge connected",
      });
      socket.write(
        '{"method":"subscribeRealtime","correlationId":"audio-realtime-bridge","payload":{"stream":"audio.nowPlaying","sendInitialSnapshot":true,"maxHz":2}}\n'
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
        module: "app.backend.audio-realtime",
        event: missingSocket ? "realtime_bridge_waiting" : "realtime_bridge_error",
        correlationId: "audio-realtime-bridge",
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

    if (typeof parsed !== "object" || parsed === null) {
      return;
    }

    const frame = parsed as {
      type?: string;
      wallTime?: string;
      monotonicMs?: number;
      payload?: {
        ownerZoneId?: number;
        trackId?: string;
        playing?: boolean;
        positionMs?: number;
        durationMs?: number;
        progressSource?: string;
      };
    };

    if (frame.type !== "audio.realtime.snapshot" || !frame.payload) {
      return;
    }

    const payload = frame.payload;
    const realtimeFrame: AudioRealtimeFrame = {
      ownerZoneId: payload.ownerZoneId ?? 0,
      trackId: payload.trackId ?? "",
      playing: payload.playing ?? false,
      positionMs: payload.positionMs ?? 0,
      durationMs: payload.durationMs ?? 0,
      progressSource: payload.progressSource ?? "pipe:ssnc/prgr",
      wallTime: frame.wallTime,
      monotonicMs: frame.monotonicMs,
    };
    this.latestFrame = realtimeFrame;

    const receivedAtMs = realtimeFrame.wallTime
      ? Date.parse(realtimeFrame.wallTime)
      : Date.now();

    this.options.broadcaster.broadcast(
      createEventEnvelope({
        source: "homepi-backend",
        topic: "modules.audio.realtime",
        event: "audio.realtime",
        correlationId: "audio-realtime-bridge",
        payload: {
          ownerZoneId: realtimeFrame.ownerZoneId,
          zoneId: realtimeFrame.ownerZoneId,
          trackId: realtimeFrame.trackId,
          playing: realtimeFrame.playing,
          positionMs: realtimeFrame.positionMs,
          durationMs: realtimeFrame.durationMs,
          receivedAtMs,
          progressSyncedAt: receivedAtMs,
        },
      })
    );

    if (realtimeFrame.wallTime) {
      this.options.coordinator.patchAndBroadcast(
        {},
        "audio-realtime-bridge",
        realtimeFrame.wallTime
      );
    }
  }
}
