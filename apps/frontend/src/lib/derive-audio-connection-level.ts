import type { AudioServiceStatus, AudioSnapshot } from "@/types/audio-types.js";

/** Aggregate audio module connection health for dashboard display. */
export type AudioConnectionLevel = "healthy" | "degraded" | "offline";

/**
 * Options for deriving dashboard connection health.
 */
export interface DeriveAudioConnectionOptions {
  /** When false, service rows from REST are not trusted yet (SSE bootstrap). */
  servicesHydrated?: boolean;
}

/**
 * Returns true when live snapshot data shows the Hi-Fi serial link is up.
 * @param snapshot - Current audio snapshot, if any.
 * @returns True when the controller serial path is present or hifiConnected is set.
 */
export function isHifiLinkUp(snapshot?: AudioSnapshot | null): boolean {
  if (snapshot?.hifiConnected === true) {
    return true;
  }
  return Boolean(snapshot?.controller?.serialPath?.trim());
}

/**
 * Returns true when PCM routing indicates an active AirPlay path.
 * @param snapshot - Current audio snapshot, if any.
 * @returns True when a zone owns the DAC or the active stack is non-empty.
 */
function hasActivePcmRoute(snapshot?: AudioSnapshot | null): boolean {
  if (!snapshot) {
    return false;
  }
  return snapshot.pcm.ownerZoneId > 0 || snapshot.pcm.activeStack.length > 0;
}

/**
 * Returns true when service rows are still the pre-REST placeholder (all offline).
 * @param services - Service rollup from the snapshot.
 * @returns True when every tracked service is offline.
 */
function isPlaceholderServiceRollup(services: AudioServiceStatus): boolean {
  const values = [
    services.hifiSerial,
    services.shairport,
    services.pcmRouter,
    services.nqptp,
    services.metadata,
  ];
  return values.every((value) => value === "offline");
}

/**
 * Derives a green / yellow / red connection level from Hi-Fi link and service health.
 * @param hifiConnected - Whether the serial link to the controller is up.
 * @param services - Optional service health rollup from the snapshot.
 * @param pcmActive - Whether live PCM routing indicates playback is active.
 * @param servicesHydrated - Whether REST service rows have been loaded.
 * @returns Connection level for status pill styling.
 */
export function deriveAudioConnectionLevel(
  hifiConnected: boolean,
  services?: AudioServiceStatus,
  pcmActive = false,
  servicesHydrated = true
): AudioConnectionLevel {
  if (!hifiConnected) {
    return "offline";
  }

  if (!services || !servicesHydrated) {
    return "healthy";
  }

  if (isPlaceholderServiceRollup(services) && (pcmActive || hifiConnected)) {
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
 * Derives dashboard connection health from the live audio snapshot.
 * @param snapshot - Current audio snapshot from REST or SSE.
 * @param options - Hydration and bootstrap options.
 * @returns Connection level for status pill styling.
 */
export function deriveAudioConnectionLevelFromSnapshot(
  snapshot?: AudioSnapshot | null,
  options: DeriveAudioConnectionOptions = {}
): AudioConnectionLevel {
  return deriveAudioConnectionLevel(
    isHifiLinkUp(snapshot),
    snapshot?.services,
    hasActivePcmRoute(snapshot),
    options.servicesHydrated ?? true
  );
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
