import { zoneInitialVolume } from "@/lib/zone-initial-volume.js";
import type { HifiZone } from "@/types/audio-types.js";

/**
 * Volume shown on zone controls (live when on or streamed, initial when off).
 * @param zone - Hi-Fi zone row.
 * @param isStreamedTo - Whether PCM router has an active AirPlay route to this zone.
 * @returns Volume 0–100 for slider display.
 */
export function zoneCardVolume(zone: HifiZone, isStreamedTo: boolean): number {
  const initialVolume = zoneInitialVolume(zone);
  if ((zone.power ?? 0) === 1 || isStreamedTo) {
    return zone.volume ?? initialVolume;
  }
  return initialVolume;
}
