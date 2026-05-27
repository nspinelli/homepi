/**
 * Request header carrying the HomePi correlation identifier.
 */
export const CORRELATION_ID_HEADER = "x-correlation-id";

/**
 * Header map accepted by request correlation helpers.
 */
export type RequestHeaders = Record<string, string | string[] | undefined> | Headers;

/**
 * Normalizes a header value to a single string.
 * @param value - Header value.
 * @returns Normalized header string or undefined.
 */
function normalizeHeaderValue(value: string | string[] | undefined): string | undefined {
  if (typeof value === "string") {
    return value;
  }
  if (Array.isArray(value)) {
    return value[0];
  }
  return undefined;
}

/**
 * Reads a header value using case-insensitive matching.
 * @param headers - Request headers.
 * @param name - Header name.
 * @returns Header value or undefined.
 */
function readHeader(headers: RequestHeaders, name: string): string | undefined {
  if (headers instanceof Headers) {
    return headers.get(name) ?? undefined;
  }

  const target = name.toLowerCase();
  for (const [key, value] of Object.entries(headers)) {
    if (key.toLowerCase() === target) {
      return normalizeHeaderValue(value);
    }
  }
  return undefined;
}

/**
 * Extracts the request correlation ID from incoming headers.
 * @param headers - Request headers.
 * @returns Correlation ID when present.
 */
export function getRequestCorrelationId(headers: RequestHeaders): string | undefined {
  return readHeader(headers, CORRELATION_ID_HEADER);
}
