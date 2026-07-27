import { type ChildProcess, spawn } from "node:child_process";

import type { Logger } from "@homepi/core-logging";

/**
 * GPIO line to watch for edge events.
 */
export interface GpioWatchLine {
  /** Logical key for this watch. */
  key: string;
  /** Linux GPIO chip device name. */
  chip: string;
  /** Offset/line number on the chip. */
  offset: number;
  /** Controller id when this is an MCP interrupt line. */
  controllerId?: string;
  /** MCP bank when this is an MCP interrupt line. */
  mcpBank?: "A" | "B";
  /** Sensor id when this is a direct GPIO sensor line. */
  sensorId?: string;
}

/**
 * Edge event callback.
 */
export type GpioEdgeHandler = (line: GpioWatchLine) => void;

/**
 * Watches GPIO lines via libgpiod `gpiomon` subprocesses (no native bindings).
 */
export class GpioLineWatcher {
  private readonly processes = new Map<string, ChildProcess>();
  private stopped = false;

  /**
   * Creates a GPIO line watcher.
   * @param logger - Structured logger.
   */
  constructor(private readonly logger: Logger) {}

  /**
   * Starts watching lines for both edges.
   * @param lines - Lines to watch.
   * @param onEdge - Edge handler.
   */
  watch(lines: GpioWatchLine[], onEdge: GpioEdgeHandler): void {
    for (const line of lines) {
      if (this.processes.has(line.key)) {
        continue;
      }

      const child = spawn(
        "gpiomon",
        ["--banner", "--num-events=0", "--format=line-name", line.chip, String(line.offset)],
        { stdio: ["ignore", "pipe", "pipe"] }
      );

      child.stdout?.setEncoding("utf8");
      child.stdout?.on("data", (chunk: string) => {
        if (this.stopped) {
          return;
        }
        if (chunk.includes("event:") || chunk.includes("rising") || chunk.includes("falling")) {
          onEdge(line);
        }
      });

      child.stderr?.on("data", (chunk: Buffer) => {
        const message = chunk.toString().trim();
        if (message) {
          this.logger.warn({
            module: "sensors",
            event: "gpio_watch_stderr",
            message,
            data: { key: line.key },
          });
        }
      });

      child.on("exit", (code) => {
        if (!this.stopped && code !== 0 && code !== null) {
          this.logger.warn({
            module: "sensors",
            event: "gpio_watch_exit",
            message: `gpiomon exited for ${line.key}`,
            data: { code },
          });
        }
      });

      this.processes.set(line.key, child);
    }
  }

  /**
   * Reads one GPIO line level via gpioget.
   * @param chip - GPIO chip name.
   * @param offset - Line offset.
   * @returns 0 or 1, or null on failure.
   */
  async readLine(chip: string, offset: number): Promise<number | null> {
    return new Promise((resolve) => {
      const child = spawn("gpioget", [chip, String(offset)]);
      let output = "";
      child.stdout?.on("data", (chunk: Buffer) => {
        output += chunk.toString();
      });
      child.on("close", (code) => {
        if (code !== 0) {
          resolve(null);
          return;
        }
        const value = Number.parseInt(output.trim(), 10);
        resolve(Number.isNaN(value) ? null : value);
      });
      child.on("error", () => resolve(null));
    });
  }

  /**
   * Stops all gpiomon subprocesses.
   */
  stop(): void {
    this.stopped = true;
    for (const child of this.processes.values()) {
      child.kill("SIGTERM");
    }
    this.processes.clear();
  }
}
