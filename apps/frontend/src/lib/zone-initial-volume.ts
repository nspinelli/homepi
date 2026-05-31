import type { HifiZone } from "@/types/audio-types.js";

/**
 * Returns the zone initial volume for forms and display (0–100).
 * Uses 50 only when the controller has not reported a value yet.
 * @param zone - Hi-Fi zone row.
 * @returns Initial volume 0–100.
 */
export function zoneInitialVolume(zone: HifiZone): number {
  return zone.initialVolume !== undefined ? zone.initialVolume : 50;
}
