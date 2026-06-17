/**
 * Converts a Hi-Fi controller volume percent (0–100) to Apple AirPlay dB (-30..0).
 * Matches the conversion used in homepi-shairport-hook.sh.
 * @param percent - Controller volume 0–100.
 * @returns AirPlay volume in dB.
 */
export function percentToAppleDb(percent: number): number {
  const clamped = Math.max(0, Math.min(100, percent));
  return -30 + (clamped / 100) * 30;
}

/**
 * Converts Apple AirPlay dB (-30..0) to a Hi-Fi controller volume percent (0–100).
 * @param db - AirPlay volume in dB.
 * @returns Controller volume 0–100.
 */
export function appleDbToPercent(db: number): number {
  const pct = ((db + 30) / 30) * 100;
  return Math.max(0, Math.min(100, Math.round(pct)));
}

/**
 * Parses a Shairport MQTT volume payload to controller percent.
 * @param payload - Raw MQTT volume string.
 * @returns Percentage or null when unparseable.
 */
export function airplayVolumePayloadToPercent(payload: string): number | null {
  if (!payload || payload === "--") {
    return null;
  }
  const airplayDb = Number.parseFloat(payload.split(",")[0] ?? "");
  if (Number.isNaN(airplayDb)) {
    return null;
  }
  if (payload.startsWith("-") || payload.includes(".")) {
    return appleDbToPercent(airplayDb);
  }
  return Math.max(0, Math.min(100, Math.round(airplayDb)));
}
