import { connect } from "node:net";

import type { EventEnvelope } from "@homepi/core-events";

/**
 * Options for the metadata Unix socket client.
 */
export interface MetadataClientOptions {
  /** Unix socket path for homepi-metadata. */
  socketPath: string;
  /** Request timeout in milliseconds. */
  timeoutMs?: number;
}

/**
 * Metadata snapshot payload from metadata_snapshot events.
 */
export interface MetadataSnapshotPayload {
  ownerZoneId: number;
  zoneId: number;
  title?: string;
  artist?: string;
  album?: string;
  clientName?: string;
  playing: boolean;
  positionMs: number;
  durationMs: number;
  hasCoverArt: boolean;
}

/**
 * Calls the native homepi-metadata Unix socket API for a one-shot snapshot.
 */
export class MetadataClient {
  private readonly socketPath: string;
  private readonly timeoutMs: number;

  /**
   * Creates a metadata socket client.
   * @param options - Client options.
   */
  constructor(options: MetadataClientOptions) {
    this.socketPath = options.socketPath;
    this.timeoutMs = options.timeoutMs ?? 5_000;
  }

  /**
   * Subscribes briefly and returns the first metadata_snapshot payload.
   * @param correlationId - Request correlation id.
   * @returns Snapshot payload or null when unavailable.
   */
  async getSnapshot(correlationId: string): Promise<MetadataSnapshotPayload | null> {
    const payload = JSON.stringify({ method: "subscribe", correlationId });

    return new Promise<MetadataSnapshotPayload | null>((resolve) => {
      const socket = connect(this.socketPath);
      let buffer = "";
      let settled = false;

      const finish = (value: MetadataSnapshotPayload | null): void => {
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
            if (parsed.event !== "metadata_snapshot") {
              continue;
            }
            const p = parsed.payload ?? {};
            finish({
              ownerZoneId: typeof p.ownerZoneId === "number" ? p.ownerZoneId : 0,
              zoneId: typeof p.zoneId === "number" ? p.zoneId : 0,
              title: typeof p.title === "string" ? p.title : undefined,
              artist: typeof p.artist === "string" ? p.artist : undefined,
              album: typeof p.album === "string" ? p.album : undefined,
              clientName: typeof p.clientName === "string" ? p.clientName : undefined,
              playing: p.playing === true,
              positionMs: typeof p.positionMs === "number" ? p.positionMs : 0,
              durationMs: typeof p.durationMs === "number" ? p.durationMs : 0,
              hasCoverArt: p.hasCoverArt === true,
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
