import { randomUUID, scryptSync, timingSafeEqual } from "node:crypto";

const KEY_PREFIX_LENGTH = 8;
const MIN_PAGING_API_KEY_LENGTH = 8;
const MAX_PAGING_API_KEY_LENGTH = 256;
const SCRYPT_KEY_LENGTH = 64;
const SCRYPT_N = 16_384;
const SCRYPT_R = 8;
const SCRYPT_P = 1;

/**
 * Validation error returned when a user-provided paging API key is invalid.
 */
export class PagingApiKeyValidationError extends Error {
  /**
   * Creates a validation error with a user-facing message.
   * @param message - Validation failure description.
   */
  constructor(message: string) {
    super(message);
    this.name = "PagingApiKeyValidationError";
  }
}

/**
 * Normalizes and validates a user-provided paging API key.
 * @param apiKey - Raw API key entered by the user.
 * @returns Trimmed API key ready for hashing.
 */
export function normalizePagingApiKey(apiKey: string): string {
  const normalized = apiKey.trim();
  if (normalized.length < MIN_PAGING_API_KEY_LENGTH) {
    throw new PagingApiKeyValidationError(
      `API key must be at least ${MIN_PAGING_API_KEY_LENGTH} characters`
    );
  }
  if (normalized.length > MAX_PAGING_API_KEY_LENGTH) {
    throw new PagingApiKeyValidationError(
      `API key must be at most ${MAX_PAGING_API_KEY_LENGTH} characters`
    );
  }
  return normalized;
}

/**
 * Derives a short display prefix for a configured paging API key.
 * @param apiKey - Raw API key entered by the user.
 * @returns Prefix safe to show in settings UI.
 */
export function derivePagingApiKeyPrefix(apiKey: string): string {
  const normalized = normalizePagingApiKey(apiKey);
  if (normalized.length <= KEY_PREFIX_LENGTH) {
    return normalized;
  }
  return `${normalized.slice(0, KEY_PREFIX_LENGTH)}…`;
}

/**
 * Hashes a paging API key using Node's scrypt implementation.
 * @param apiKey - Raw API key to hash.
 * @returns Encoded hash string containing algorithm params and salt.
 */
export async function hashPagingApiKey(apiKey: string): Promise<string> {
  const normalized = normalizePagingApiKey(apiKey);
  const salt = randomUUID();
  const hash = scryptSync(normalized, salt, SCRYPT_KEY_LENGTH, {
    N: SCRYPT_N,
    r: SCRYPT_R,
    p: SCRYPT_P,
  });
  return [
    "scrypt",
    String(SCRYPT_N),
    String(SCRYPT_R),
    String(SCRYPT_P),
    salt,
    hash.toString("hex"),
  ].join("$");
}

/**
 * Verifies a paging API key against a previously generated scrypt hash.
 * @param apiKey - Raw API key presented by the caller.
 * @param encodedHash - Stored encoded hash string.
 * @returns True when the provided key matches the stored hash.
 */
export async function verifyPagingApiKey(
  apiKey: string,
  encodedHash: string | null | undefined
): Promise<boolean> {
  if (!encodedHash) {
    return false;
  }

  const parts = encodedHash.split("$");
  if (parts.length !== 6 || parts[0] !== "scrypt") {
    return false;
  }

  const n = Number(parts[1]);
  const r = Number(parts[2]);
  const p = Number(parts[3]);
  const salt = parts[4];
  const expectedHex = parts[5];
  if (!Number.isFinite(n) || !Number.isFinite(r) || !Number.isFinite(p) || !salt || !expectedHex) {
    return false;
  }

  const derived = scryptSync(apiKey.trim(), salt, expectedHex.length / 2, {
    N: n,
    r,
    p,
  });
  const expected = Buffer.from(expectedHex, "hex");
  if (derived.length !== expected.length) {
    return false;
  }

  return timingSafeEqual(derived, expected);
}
