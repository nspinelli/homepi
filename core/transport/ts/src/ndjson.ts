/**
 * Encodes a value as a single NDJSON line (no trailing newline in return value).
 * @param value - Object to encode.
 * @returns NDJSON line string.
 */
export function encodeNdjsonLine(value: unknown): string {
  return JSON.stringify(value);
}

/**
 * Decodes one NDJSON line into an object.
 * @param line - Single line without trailing newline.
 * @returns Parsed object.
 */
export function decodeNdjsonLine<T = unknown>(line: string): T {
  return JSON.parse(line.trim()) as T;
}

/**
 * Splits a buffer/string into complete NDJSON lines.
 * @param buffer - Accumulated stream buffer.
 * @returns Tuple of complete lines and remaining partial buffer.
 */
export function splitNdjsonLines(buffer: string): [string[], string] {
  const lines = buffer.split("\n");
  const remainder = lines.pop() ?? "";
  const complete = lines.filter((l) => l.trim().length > 0);
  return [complete, remainder];
}
