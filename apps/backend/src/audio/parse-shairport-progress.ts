/** Default AirPlay sample rate used by Shairport progress topics. */
const DEFAULT_SAMPLE_RATE_HZ = 44_100;

/**
 * Converts an RTP sample span to milliseconds.
 * @param start - Start RTP sample.
 * @param end - End RTP sample.
 * @param sampleRateHz - Audio sample rate.
 * @returns Duration in milliseconds.
 */
function rtpSpanToMs(start: number, end: number, sampleRateHz: number): number {
  if (sampleRateHz <= 0 || end < start) {
    return 0;
  }
  return Math.floor(((end - start) * 1000) / sampleRateHz);
}

/**
 * Parsed Shairport `ssnc/prgr` progress payload.
 */
export interface ShairportPrgrProgress {
  /** Current position in milliseconds. */
  positionMs: number;
  /** Track duration in milliseconds. */
  durationMs: number;
}

/**
 * Parses a Shairport `start/current/end` RTP progress payload.
 * @param payload - Raw MQTT payload.
 * @param sampleRateHz - Audio sample rate.
 * @returns Parsed progress or null when invalid.
 */
export function parseShairportPrgrProgress(
  payload: string,
  sampleRateHz = DEFAULT_SAMPLE_RATE_HZ
): ShairportPrgrProgress | null {
  const tokens = payload.split("/");
  if (tokens.length !== 3) {
    return null;
  }

  const start = Number(tokens[0]);
  const current = Number(tokens[1]);
  const end = Number(tokens[2]);
  if (!Number.isFinite(start) || !Number.isFinite(current) || !Number.isFinite(end)) {
    return null;
  }

  const positionMs = rtpSpanToMs(start, current, sampleRateHz);
  const durationMs = rtpSpanToMs(start, end, sampleRateHz);
  if (positionMs <= 0 && durationMs <= 0) {
    return null;
  }

  return { positionMs, durationMs };
}

/**
 * Parses a Shairport `current/wall_clock_ns` frame position payload.
 * @param payload - Raw MQTT payload.
 * @param progressStartRtp - RTP sample captured from `phb0` or `prgr`.
 * @param sampleRateHz - Audio sample rate.
 * @returns Position in milliseconds or null when invalid.
 */
export function parseShairportPhbtPosition(
  payload: string,
  progressStartRtp: number,
  sampleRateHz = DEFAULT_SAMPLE_RATE_HZ
): number | null {
  const tokens = payload.split("/");
  if (tokens.length === 0) {
    return null;
  }
  const current = Number(tokens[0]);
  const start = progressStartRtp > 0 ? progressStartRtp : current;
  const positionMs = rtpSpanToMs(start, current, sampleRateHz);
  return positionMs > 0 ? positionMs : null;
}

/**
 * Parses a Shairport `core/astm` duration payload.
 * @param payload - Raw MQTT bytes.
 * @returns Duration in milliseconds or null when invalid.
 */
export function parseShairportAstmDuration(payload: Buffer): number | null {
  if (payload.length < 4) {
    return null;
  }
  const durationMs =
    (payload[0]! << 24) | (payload[1]! << 16) | (payload[2]! << 8) | payload[3]!;
  return durationMs > 0 ? durationMs : null;
}
