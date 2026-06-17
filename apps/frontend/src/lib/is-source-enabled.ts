import type { HifiSource } from "@/types/audio-types.js";

/**
 * Returns true when the controller reports the source as enabled.
 * Missing or non-1 values are treated as disabled.
 * @param source - Hi-Fi source row.
 * @returns Whether the source is enabled.
 */
export function isSourceEnabled(source: HifiSource): boolean {
  return source.enabled === 1;
}
