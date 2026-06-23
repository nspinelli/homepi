import { connect } from "node:net";

import type { AudioRealtimeFrame } from "./audio-realtime-bridge.js";

/**
 * Reads the latest progress frame from audio-realtime.sock with a short timeout.
 * @param socketPath - Unix socket path.
 * @returns Latest realtime frame or null when unavailable.
 */
export async function readAudioRealtimeSnapshot(
  socketPath: string
): Promise<AudioRealtimeFrame | null> {
  return new Promise((resolve) => {
    const socket = connect(socketPath);
    let buffer = "";
    let settled = false;

    const finish = (frame: AudioRealtimeFrame | null): void => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timeout);
      socket.destroy();
      resolve(frame);
    };

    const timeout = setTimeout(() => finish(null), 500);

    socket.on("connect", () => {
      socket.write(
        '{"method":"subscribeRealtime","correlationId":"audio-snapshot","payload":{"stream":"audio.nowPlaying","sendInitialSnapshot":true}}\n'
      );
    });

    socket.on("data", (chunk) => {
      buffer += chunk.toString("utf8");
      const lines = buffer.split("\n");
      for (const line of lines) {
        if (!line.trim()) {
          continue;
        }
        try {
          const parsed = JSON.parse(line) as {
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
          if (parsed.type !== "audio.realtime.snapshot" || !parsed.payload) {
            continue;
          }
          finish({
            ownerZoneId: parsed.payload.ownerZoneId ?? 0,
            trackId: parsed.payload.trackId ?? "",
            playing: parsed.payload.playing ?? false,
            positionMs: parsed.payload.positionMs ?? 0,
            durationMs: parsed.payload.durationMs ?? 0,
            progressSource: parsed.payload.progressSource ?? "pipe:ssnc/prgr",
            wallTime: parsed.wallTime,
            monotonicMs: parsed.monotonicMs,
          });
          return;
        } catch {
          continue;
        }
      }
    });

    socket.on("error", () => finish(null));
    socket.on("close", () => finish(null));
  });
}
