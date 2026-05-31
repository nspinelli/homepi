import type { HifiZone } from "@/types/audio-types.js";

/**
 * Returns true when the controller reports the zone as enabled.
 * Missing or non-1 values are treated as disabled (avoids defaulting unknown rows to enabled).
 * @param zone - Hi-Fi zone row.
 * @returns Whether the zone is enabled.
 */
export function isZoneEnabled(zone: HifiZone): boolean {
  return zone.enabled === 1;
}
