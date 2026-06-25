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
 * One play-history row from metadata service.
 */
export interface PlayHistoryEntry {
  id: number;
  zoneId: number;
  title?: string;
  artist?: string;
  album?: string;
  trackId?: string;
  clientName?: string;
  coverArtId?: string;
  durationMs: number;
  playedAt?: string;
}

/**
 * Play-history list payload from metadata service.
 */
export interface PlayHistoryPayload {
  limit: number;
  entries: PlayHistoryEntry[];
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
  clientModel?: string;
  coverArtUrl?: string;
  coverArtId?: string;
  metadataQuality?: string;
  trackId?: string;
  playing: boolean;
  positionMs: number;
  durationMs: number;
  hasCoverArt: boolean;
  updatedAt?: string;
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
              clientModel: typeof p.clientModel === "string" ? p.clientModel : undefined,
              coverArtUrl: typeof p.coverArtUrl === "string" ? p.coverArtUrl : undefined,
              coverArtId: typeof p.coverArtId === "string" ? p.coverArtId : undefined,
              metadataQuality:
                typeof p.metadataQuality === "string" ? p.metadataQuality : undefined,
              trackId: typeof p.trackId === "string" ? p.trackId : undefined,
              playing: p.playing === true,
              positionMs: typeof p.positionMs === "number" ? p.positionMs : 0,
              durationMs: typeof p.durationMs === "number" ? p.durationMs : 0,
              hasCoverArt: p.hasCoverArt === true,
              updatedAt: typeof p.updatedAt === "string" ? p.updatedAt : undefined,
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

  /**
   * Requests the recent play-history list from metadata service.
   * @param correlationId - Request correlation id.
   * @param limit - Maximum rows to return.
   * @returns History payload or null when unavailable.
   */
  async getHistory(correlationId: string, limit = 20): Promise<PlayHistoryPayload | null> {
    const payload = JSON.stringify({ method: "history", correlationId, limit });

    return new Promise<PlayHistoryPayload | null>((resolve) => {
      const socket = connect(this.socketPath);
      let buffer = "";
      let settled = false;

      const finish = (value: PlayHistoryPayload | null): void => {
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
            if (parsed.event !== "play_history_snapshot") {
              continue;
            }
            const p = parsed.payload ?? {};
            const rawEntries = Array.isArray(p.entries) ? p.entries : [];
            const entries: PlayHistoryEntry[] = [];
            for (const entry of rawEntries) {
              if (!entry || typeof entry !== "object") {
                continue;
              }
              const row = entry as Record<string, unknown>;
              entries.push({
                id: typeof row.id === "number" ? row.id : 0,
                zoneId: typeof row.zoneId === "number" ? row.zoneId : 0,
                title: typeof row.title === "string" ? row.title : undefined,
                artist: typeof row.artist === "string" ? row.artist : undefined,
                album: typeof row.album === "string" ? row.album : undefined,
                trackId: typeof row.trackId === "string" ? row.trackId : undefined,
                clientName: typeof row.clientName === "string" ? row.clientName : undefined,
                durationMs: typeof row.durationMs === "number" ? row.durationMs : 0,
                playedAt: typeof row.playedAt === "string" ? row.playedAt : undefined,
              });
            }
            finish({
              limit: typeof p.limit === "number" ? p.limit : entries.length,
              entries,
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
