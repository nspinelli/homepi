import { readFile } from "node:fs/promises";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = dirname(fileURLToPath(import.meta.url));

/** Candidate locations for the static Piper voice catalog JSON. */
const CATALOG_PATH_CANDIDATES = [
  join(__dirname, "..", "..", "assets", "data", "paging-voice-catalog.json"),
  join(__dirname, "..", "..", "..", "src", "assets", "data", "paging-voice-catalog.json"),
  join(process.cwd(), "src", "assets", "data", "paging-voice-catalog.json"),
  join(process.cwd(), "dist", "assets", "data", "paging-voice-catalog.json"),
] as const;

/** Root directory for installed Piper voice model files. */
export const PAGING_VOICE_STORAGE_DIR = "/var/lib/homepi/paging/voices";

/** Hugging Face base URL for Piper voice artifacts. */
const PIPER_VOICES_BASE_URL =
  "https://huggingface.co/rhasspy/piper-voices/resolve/main";

/** Default Hugging Face sample file for single-speaker Piper voices. */
const PIPER_VOICE_SAMPLE_FILE = "samples/speaker_0.mp3";

/**
 * Catalog voice metadata used by the paging voice browser.
 */
export interface PagingVoiceCatalogEntry {
  /** Stable Piper voice identifier. */
  voiceId: string;
  /** Friendly display name. */
  displayName: string;
  /** BCP-47 language code. */
  languageCode: string;
  /** Human-readable accent label for UI grouping. */
  accent: string;
  /** Voice quality tier. */
  quality: string;
  /** True when the voice ships with HomePi. */
  isBundled: boolean;
  /** True when a browser-playable sample exists. */
  sampleAvailable: boolean;
  /** Optional remote sample URL. */
  sampleUrl?: string;
}

/**
 * Parsed Piper voice identifier segments.
 */
export interface ParsedPiperVoiceId {
  /** Language code prefix, e.g. `en`. */
  lang: string;
  /** Region code, e.g. `US`. */
  region: string;
  /** Speaker slug, e.g. `lessac`. */
  speaker: string;
  /** Quality tier, e.g. `medium`. */
  quality: string;
}

/**
 * Local file paths for an installed Piper voice.
 */
export interface PagingVoiceFilePaths {
  /** Absolute ONNX model path. */
  modelPath: string;
  /** Absolute JSON config path. */
  configPath: string;
  /** Remote ONNX download URL. */
  modelUrl: string;
  /** Remote JSON config download URL. */
  configUrl: string;
}

/**
 * Voice catalog document loaded from JSON assets.
 */
export interface PagingVoiceCatalogDocument {
  /** Catalog voice rows. */
  voices: PagingVoiceCatalogEntry[];
}

/**
 * Parses a Piper voice id into URL path segments.
 * @param voiceId - Piper voice identifier.
 * @returns Parsed segments or null when the id is invalid.
 */
export function parsePiperVoiceId(voiceId: string): ParsedPiperVoiceId | null {
  const match = /^(en)_(US|GB)-(.+)-(low|medium|high|x_low)$/.exec(voiceId);
  if (!match) {
    return null;
  }

  return {
    lang: match[1],
    region: match[2],
    speaker: match[3],
    quality: match[4],
  };
}

/**
 * Builds local and remote file paths for a Piper voice id.
 * @param voiceId - Piper voice identifier.
 * @returns Resolved voice file paths.
 */
export function resolvePagingVoiceFilePaths(voiceId: string): PagingVoiceFilePaths {
  const parsed = parsePiperVoiceId(voiceId);
  if (!parsed) {
    throw new Error(`Unsupported voice id: ${voiceId}`);
  }

  const locale = `${parsed.lang}_${parsed.region}`;
  const remoteBase = `${PIPER_VOICES_BASE_URL}/${parsed.lang}/${locale}/${parsed.speaker}/${parsed.quality}`;

  return {
    modelPath: `${PAGING_VOICE_STORAGE_DIR}/${voiceId}.onnx`,
    configPath: `${PAGING_VOICE_STORAGE_DIR}/${voiceId}.onnx.json`,
    modelUrl: `${remoteBase}/${voiceId}.onnx`,
    configUrl: `${remoteBase}/${voiceId}.onnx.json`,
  };
}

/**
 * Builds a browser-playable sample URL for a Piper catalog voice.
 * @param voiceId - Piper voice identifier.
 * @returns Remote sample URL or null when the voice id is unsupported.
 */
export function buildPiperVoiceSampleUrl(voiceId: string): string | null {
  const parsed = parsePiperVoiceId(voiceId);
  if (!parsed) {
    return null;
  }

  const locale = `${parsed.lang}_${parsed.region}`;
  return `${PIPER_VOICES_BASE_URL}/${parsed.lang}/${locale}/${parsed.speaker}/${parsed.quality}/${PIPER_VOICE_SAMPLE_FILE}`;
}

/**
 * Resolves the sample URL shown in the voice catalog UI.
 * @param voice - Catalog voice row.
 * @returns Sample URL when available, otherwise null.
 */
export function resolvePagingVoiceSampleUrl(voice: PagingVoiceCatalogEntry): string | null {
  if (!voice.sampleAvailable) {
    return null;
  }
  return buildPiperVoiceSampleUrl(voice.voiceId) ?? voice.sampleUrl ?? null;
}

/**
 * Returns true for English catalog voices.
 * @param voice - Catalog voice row.
 * @returns True when the voice should appear in the English picker.
 */
export function isEnglishVoice(voice: PagingVoiceCatalogEntry): boolean {
  return voice.languageCode.toLowerCase().startsWith("en") || voice.voiceId.startsWith("en_");
}

/**
 * Reads the paging voice catalog from bundled backend assets.
 * @returns Parsed catalog document.
 */
export async function readPagingVoiceCatalog(): Promise<PagingVoiceCatalogDocument> {
  for (const candidate of CATALOG_PATH_CANDIDATES) {
    try {
      const raw = await readFile(candidate, "utf8");
      const parsed = JSON.parse(raw) as PagingVoiceCatalogDocument;
      if (Array.isArray(parsed.voices)) {
        return parsed;
      }
    } catch {
      continue;
    }
  }

  return { voices: [] };
}

/**
 * Finds a catalog voice by id.
 * @param voiceId - Voice identifier.
 * @returns Matching catalog row or null.
 */
export async function findCatalogVoice(
  voiceId: string
): Promise<PagingVoiceCatalogEntry | null> {
  const catalog = await readPagingVoiceCatalog();
  return catalog.voices.find((voice) => voice.voiceId === voiceId) ?? null;
}
