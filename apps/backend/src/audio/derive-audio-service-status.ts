import type { SystemHealthSnapshot } from "../health/health-client.js";

/** Registry service names mapped to audio snapshot service keys. */
const AUDIO_SERVICE_NAMES = {
  hifiSerial: "homepi-hifi-serial",
  shairport: "homepi-shairport-supervisor",
  pcmRouter: "homepi-pcm-router",
  nqptp: "homepi-nqptp",
  metadata: "homepi-metadata",
} as const;

/**
 * Normalizes a homepi-health service status for audio UI rollups.
 * @param status - Raw health observer status.
 * @returns Audio snapshot service status string.
 */
function normalizeServiceStatus(status: string | undefined): string {
  if (status === "healthy") {
    return "healthy";
  }
  if (status === "degraded") {
    return "degraded";
  }
  return "offline";
}

/**
 * Derives audio module service rollups from a homepi-health snapshot.
 * @param snapshot - Health snapshot from homepi-health, if available.
 * @returns Service status map for the audio dashboard snapshot.
 */
export function deriveAudioServiceStatus(
  snapshot: SystemHealthSnapshot | null
): {
  hifiSerial: string;
  shairport: string;
  pcmRouter: string;
  nqptp: string;
  metadata: string;
} {
  const lookup = (serviceName: string): string => {
    const entry = snapshot?.services.find((service) => service.service === serviceName);
    return normalizeServiceStatus(entry?.status);
  };

  return {
    hifiSerial: lookup(AUDIO_SERVICE_NAMES.hifiSerial),
    shairport: lookup(AUDIO_SERVICE_NAMES.shairport),
    pcmRouter: lookup(AUDIO_SERVICE_NAMES.pcmRouter),
    nqptp: lookup(AUDIO_SERVICE_NAMES.nqptp),
    metadata: lookup(AUDIO_SERVICE_NAMES.metadata),
  };
}
