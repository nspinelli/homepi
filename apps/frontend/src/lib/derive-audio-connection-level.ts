import type { AudioServiceStatus } from "@/types/audio-types.js";

/** Aggregate audio module connection health for dashboard display. */
export type AudioConnectionLevel = "healthy" | "degraded" | "offline";

/**
 * Derives a green / yellow / red connection level from Hi-Fi link and service health.
 * @param hifiConnected - Whether the serial link to the controller is up.
 * @param services - Optional service health rollup from the snapshot.
 * @returns Connection level for status pill styling.
 */
export function deriveAudioConnectionLevel(
  hifiConnected: boolean,
  services?: AudioServiceStatus
): AudioConnectionLevel {
  if (!hifiConnected) {
    return "offline";
  }

  if (!services) {
    return "healthy";
  }

  const values = [
    services.hifiSerial,
    services.shairport,
    services.pcmRouter,
    services.nqptp,
    services.metadata,
  ];

  if (values.some((value) => value === "offline" || value === "failed")) {
    return "offline";
  }

  if (values.some((value) => value === "degraded")) {
    return "degraded";
  }

  return "healthy";
}

/**
 * Human-readable label for an audio connection level.
 * @param level - Derived connection level.
 * @returns Status label shown on the dashboard card.
 */
export function audioConnectionLabel(level: AudioConnectionLevel): string {
  switch (level) {
    case "healthy":
      return "Connected";
    case "degraded":
      return "Degraded";
    case "offline":
      return "Disconnected";
  }
}
