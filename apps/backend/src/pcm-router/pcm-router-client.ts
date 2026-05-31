import { connect } from "node:net";

import type { EventEnvelope } from "@homepi/core-events";

/**
 * Options for the PCM router Unix socket client.
 */
export interface PcmRouterClientOptions {
  /** Unix socket path for homepi-pcm-router. */
  socketPath: string;
  /** Request timeout in milliseconds. */
  timeoutMs?: number;
}

/**
 * PCM router snapshot payload from subscribe.
 */
export interface PcmRouterSnapshotPayload {
  ownerZoneId: number;
  activeStack: number[];
  dacState: string;
  sourceType?: string;
}

/**
 * Calls the native homepi-pcm-router Unix socket API for a one-shot snapshot.
 */
export class PcmRouterClient {
  private readonly socketPath: string;
  private readonly timeoutMs: number;

  /**
   * Creates a PCM router socket client.
   * @param options - Client options.
   */
  constructor(options: PcmRouterClientOptions) {
    this.socketPath = options.socketPath;
    this.timeoutMs = options.timeoutMs ?? 5_000;
  }

  /**
   * Subscribes briefly and returns the first pcm_router_snapshot payload.
   * @param correlationId - Request correlation id.
   * @returns Snapshot payload or null when unavailable.
   */
  async getSnapshot(correlationId: string): Promise<PcmRouterSnapshotPayload | null> {
    const payload = JSON.stringify({ method: "subscribe", correlationId });

    return new Promise<PcmRouterSnapshotPayload | null>((resolve) => {
      const socket = connect(this.socketPath);
      let buffer = "";
      let settled = false;

      const finish = (value: PcmRouterSnapshotPayload | null): void => {
        if (settled) {
          return;
        }
        settled = true;
        clearTimeout(timer);
        socket.destroy();
        resolve(value);
      };

      const timer = setTimeout(() => finish(null), this.timeoutMs);

      socket.on("error", () => finish(null));

      socket.on("data", (chunk) => {
        buffer += chunk.toString("utf8");
        const lines = buffer.split("\n");
        buffer = lines.pop() ?? "";

        for (const line of lines) {
          if (!line.trim()) {
            continue;
          }
          try {
            const parsed = JSON.parse(line) as EventEnvelope & {
              payload?: Record<string, unknown>;
            };
            if (parsed.event !== "pcm_router_snapshot") {
              continue;
            }
            const p = parsed.payload ?? {};
            const stack = Array.isArray(p.activeStack)
              ? (p.activeStack as number[])
              : [];
            finish({
              ownerZoneId: typeof p.ownerZoneId === "number" ? p.ownerZoneId : 0,
              activeStack: stack,
              dacState: typeof p.dacState === "string" ? p.dacState : "unknown",
              sourceType:
                typeof p.sourceType === "string" ? p.sourceType : undefined,
            });
            return;
          } catch {
            continue;
          }
        }
      });

      socket.on("connect", () => {
        socket.write(`${payload}\n`);
      });
    });
  }
}
