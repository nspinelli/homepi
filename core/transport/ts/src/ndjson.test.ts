import { describe, expect, it } from "vitest";

import { encodeNdjsonLine, splitNdjsonLines } from "./ndjson.js";

describe("ndjson", () => {
  it("terminates encoded lines with a newline", () => {
    const line = encodeNdjsonLine({ ok: true });
    expect(line.endsWith("\n")).toBe(true);
    expect(splitNdjsonLines(line)[0]).toEqual(['{"ok":true}']);
  });

  it("splits multiple complete lines from a buffer", () => {
    const buffer = encodeNdjsonLine({ a: 1 }) + encodeNdjsonLine({ b: 2 });
    const [lines, remainder] = splitNdjsonLines(buffer);
    expect(lines).toEqual(['{"a":1}', '{"b":2}']);
    expect(remainder).toBe("");
  });
});
