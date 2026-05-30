import { spawn, type ChildProcess } from "node:child_process";

import { createEventEnvelope } from "@homepi/core-events";
import type { LogLevel, LogMessage } from "@homepi/core-logging";
import type { Logger } from "@homepi/core-logging";

import type { EventBroadcaster } from "../event-broadcaster.js";

const LOG_TOPIC = "system.logging";
const LOG_EVENT = "log_record";

const JOURNAL_UNITS = [
  "homepi-backend.service",
  "homepi-usb-devices.service",
  "homepi-hifi-serial.service",
  "homepi-pcm-router.service",
  "homepi-nqptp.service",
  "homepi-shairport-supervisor.service",
  "homepi-shairport@.service",
  "homepi-metadata@.service",
] as const;

/**
 * Options for the journald → SSE log bridge.
 */
export interface JournalLogBridgeOptions {
  /** Structured logger for bridge lifecycle. */
  logger: Logger;
  /** SSE broadcaster receiving log_record envelopes. */
  broadcaster: EventBroadcaster;
}

/**
 * Tails structured JSON logs from HomePi systemd units into SSE.
 */
export class JournalLogBridge {
  private child: ChildProcess | null = null;
  private buffer = "";
  private stopped = false;

  /**
   * @param options - Bridge configuration.
   */
  constructor(private readonly options: JournalLogBridgeOptions) {}

  /**
   * Starts following journald output for HomePi services.
   */
  start(): void {
    if (this.child) {
      return;
    }

    this.stopped = false;
    const args = [
      "-f",
      "-o",
      "cat",
      "--since",
      "now",
      ...JOURNAL_UNITS.flatMap((unit) => ["-u", unit]),
    ];

    this.child = spawn("journalctl", args, { stdio: ["ignore", "pipe", "pipe"] });

    this.child.stdout?.on("data", (chunk: Buffer) => {
      this.onData(chunk.toString("utf8"));
    });

    this.child.stderr?.on("data", (chunk: Buffer) => {
      this.options.logger.warn({
        module: "app.backend.logging",
        event: "journal_bridge_stderr",
        message: "journalctl stderr output",
        data: { line: chunk.toString("utf8").trim() },
      });
    });

    this.child.on("error", (error: Error) => {
      this.options.logger.warn({
        module: "app.backend.logging",
        event: "journal_bridge_spawn_error",
        message: "Failed to spawn journalctl",
        data: { error: error.message },
      });
      this.scheduleRestart();
    });

    this.child.on("close", (code) => {
      this.child = null;
      if (!this.stopped) {
        this.options.logger.warn({
          module: "app.backend.logging",
          event: "journal_bridge_closed",
          message: "journalctl follower exited",
          data: { code },
        });
        this.scheduleRestart();
      }
    });

    this.options.logger.info({
      module: "app.backend.logging",
      event: "journal_bridge_started",
      message: "Journal log bridge started",
      data: { units: JOURNAL_UNITS },
    });
  }

  /**
   * Stops the journal follower process.
   */
  stop(): void {
    this.stopped = true;
    if (this.child) {
      this.child.kill("SIGTERM");
      this.child = null;
    }
  }

  private scheduleRestart(): void {
    if (this.stopped) {
      return;
    }
    setTimeout(() => {
      if (!this.stopped && !this.child) {
        this.start();
      }
    }, 5_000);
  }

  private onData(chunk: string): void {
    this.buffer += chunk;
    let newline = this.buffer.indexOf("\n");
    while (newline >= 0) {
      const line = this.buffer.slice(0, newline).trim();
      this.buffer = this.buffer.slice(newline + 1);
      this.handleLine(line);
      newline = this.buffer.indexOf("\n");
    }
  }

  private handleLine(line: string): void {
    if (!line.startsWith("{")) {
      return;
    }

    let parsed: unknown;
    try {
      parsed = JSON.parse(line);
    } catch {
      return;
    }

    const log = parsed as Partial<LogMessage>;
    if (
      typeof log.ts !== "string" ||
      typeof log.service !== "string" ||
      typeof log.module !== "string" ||
      typeof log.level !== "string" ||
      typeof log.event !== "string" ||
      typeof log.correlationId !== "string" ||
      typeof log.message !== "string"
    ) {
      return;
    }

    const envelope = createEventEnvelope({
      source: log.service,
      topic: LOG_TOPIC,
      event: LOG_EVENT,
      correlationId: log.correlationId,
      timestamp: log.ts,
      payload: {
        ts: log.ts,
        service: log.service,
        module: log.module,
        level: log.level as LogLevel,
        event: log.event,
        correlationId: log.correlationId,
        message: log.message,
        data: log.data ?? {},
      },
    });

    this.options.broadcaster.broadcast(envelope);
  }
}
