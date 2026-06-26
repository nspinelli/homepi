/**
 * Result of validating and storing an uploaded chime WAV.
 */
export interface SavedChimeUpload {
  /** Stable chime identifier used in DB and filename. */
  chimeId: string;
  /** Display name shown in the UI. */
  displayName: string;
  /** Absolute path where the WAV was written. */
  filePath: string;
  /** Detected duration in milliseconds. */
  durationMs: number;
  /** Stored file size in bytes. */
  sizeBytes: number;
}

/** Maximum chime upload size in bytes (500 KB). */
export const MAX_CHIME_UPLOAD_BYTES = 512_000;

/** Maximum chime duration in milliseconds (3 seconds). */
export const MAX_CHIME_DURATION_MS = 3_000;

/**
 * Parses PCM WAV duration from a buffer.
 * @param buffer - WAV file bytes.
 * @returns Duration in milliseconds.
 */
export function getWavDurationMs(buffer: Buffer): number {
  if (buffer.length < 44 || buffer.toString("ascii", 0, 4) !== "RIFF") {
    throw new Error("Invalid WAV file: missing RIFF header");
  }
  if (buffer.toString("ascii", 8, 12) !== "WAVE") {
    throw new Error("Invalid WAV file: WAVE format required");
  }

  let offset = 12;
  let sampleRate = 0;
  let bitsPerSample = 0;
  let channels = 0;
  let dataSize = 0;

  while (offset + 8 <= buffer.length) {
    const chunkId = buffer.toString("ascii", offset, offset + 4);
    const chunkSize = buffer.readUInt32LE(offset + 4);
    const chunkDataStart = offset + 8;

    if (chunkId === "fmt ") {
      channels = buffer.readUInt16LE(chunkDataStart + 2);
      sampleRate = buffer.readUInt32LE(chunkDataStart + 4);
      bitsPerSample = buffer.readUInt16LE(chunkDataStart + 14);
    } else if (chunkId === "data") {
      dataSize = chunkSize;
      break;
    }

    offset = chunkDataStart + chunkSize + (chunkSize % 2);
  }

  if (sampleRate <= 0 || bitsPerSample <= 0 || channels <= 0 || dataSize <= 0) {
    throw new Error("Invalid WAV file: unable to read audio format");
  }

  const bytesPerSecond = (sampleRate * channels * bitsPerSample) / 8;
  return Math.round((dataSize / bytesPerSecond) * 1000);
}

/**
 * Builds a filesystem-safe chime id from a display name.
 * @param displayName - Human-readable chime name.
 * @returns Slugged chime identifier.
 */
export function buildChimeId(displayName: string): string {
  const slug = displayName
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 40);
  const suffix = Date.now().toString(36);
  return slug.length > 0 ? `${slug}-${suffix}` : `chime-${suffix}`;
}

/**
 * Validates and stores an uploaded chime WAV on disk.
 * @param displayName - Display name from the upload request.
 * @param wavBuffer - Raw WAV bytes.
 * @param chimesRoot - Directory where chime files are stored.
 * @returns Saved chime metadata.
 */
export async function saveChimeUpload(
  displayName: string,
  wavBuffer: Buffer,
  chimesRoot: string
): Promise<SavedChimeUpload> {
  const trimmedName = displayName.trim();
  if (!trimmedName) {
    throw new Error("displayName is required");
  }
  if (wavBuffer.length === 0) {
    throw new Error("WAV file is empty");
  }
  if (wavBuffer.length > MAX_CHIME_UPLOAD_BYTES) {
    throw new Error(`WAV file exceeds ${MAX_CHIME_UPLOAD_BYTES} bytes`);
  }

  const durationMs = getWavDurationMs(wavBuffer);
  if (durationMs > MAX_CHIME_DURATION_MS) {
    throw new Error(`WAV duration exceeds ${MAX_CHIME_DURATION_MS / 1000} seconds`);
  }

  const chimeId = buildChimeId(trimmedName);
  const filePath = `${chimesRoot}/${chimeId}.wav`;

  const { mkdir, writeFile } = await import("node:fs/promises");
  await mkdir(chimesRoot, { recursive: true });
  await writeFile(filePath, wavBuffer);

  return {
    chimeId,
    displayName: trimmedName,
    filePath,
    durationMs,
    sizeBytes: wavBuffer.length,
  };
}
