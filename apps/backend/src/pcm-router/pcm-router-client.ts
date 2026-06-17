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
 * PCM profile tuple from pcm_router_snapshot v2.
 */
export interface PcmProfileTuple {
  sampleRate: number;
  channels: number;
  sampleFormat: string;
}

/**
 * PCM router snapshot payload from subscribe (v2).
 */
export interface PcmRouterSnapshotPayload {
  ownerZoneId: number;
  activeStack: number[];
  dacState: string;
  profileMode?: string;
  profileStatus?: string;
  loopbackProfile?: PcmProfileTuple;
  dacProfile?: PcmProfileTuple;
  alsaDacDevice?: string;
  profileRevision?: number;
  profileSource?: string;
  audioBridgeState?: string;
  stats?: {
    captureXruns: number;
    playbackXruns: number;
    framesCopied: number;
  };
  /** @deprecated Metadata is no longer emitted by pcm-router. */
  sourceType?: string;
  /** @deprecated */
  playing?: boolean;
  /** @deprecated */
  positionMs?: number;
  /** @deprecated */
  durationMs?: number;
  /** @deprecated */
  title?: string;
  /** @deprecated */
  artist?: string;
  /** @deprecated */
  album?: string;
  /** @deprecated */
  clientName?: string;
  /** @deprecated */
  hasCoverArt?: boolean;
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
            const parseTuple = (value: unknown): PcmProfileTuple | undefined => {
              if (typeof value !== "object" || value === null) {
                return undefined;
              }
              const tuple = value as Record<string, unknown>;
              if (
                typeof tuple.sampleRate !== "number" ||
                typeof tuple.channels !== "number" ||
                typeof tuple.sampleFormat !== "string"
              ) {
                return undefined;
              }
              return {
                sampleRate: tuple.sampleRate,
                channels: tuple.channels,
                sampleFormat: tuple.sampleFormat,
              };
            };
            finish({
              ownerZoneId: typeof p.ownerZoneId === "number" ? p.ownerZoneId : 0,
              activeStack: stack,
              dacState: typeof p.dacState === "string" ? p.dacState : "unknown",
              profileMode: typeof p.profileMode === "string" ? p.profileMode : undefined,
              profileStatus:
                typeof p.profileStatus === "string" ? p.profileStatus : undefined,
              loopbackProfile: parseTuple(p.loopbackProfile),
              dacProfile: parseTuple(p.dacProfile),
              alsaDacDevice:
                typeof p.alsaDacDevice === "string" ? p.alsaDacDevice : undefined,
              profileRevision:
                typeof p.profileRevision === "number" ? p.profileRevision : undefined,
              profileSource:
                typeof p.profileSource === "string" ? p.profileSource : undefined,
              audioBridgeState:
                typeof p.audioBridgeState === "string" ? p.audioBridgeState : undefined,
              stats:
                typeof p.stats === "object" && p.stats !== null
                  ? {
                      captureXruns:
                        typeof (p.stats as { captureXruns?: unknown }).captureXruns ===
                        "number"
                          ? (p.stats as { captureXruns: number }).captureXruns
                          : 0,
                      playbackXruns:
                        typeof (p.stats as { playbackXruns?: unknown }).playbackXruns ===
                        "number"
                          ? (p.stats as { playbackXruns: number }).playbackXruns
                          : 0,
                      framesCopied:
                        typeof (p.stats as { framesCopied?: unknown }).framesCopied ===
                        "number"
                          ? (p.stats as { framesCopied: number }).framesCopied
                          : 0,
                    }
                  : undefined,
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
