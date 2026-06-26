import { describe, expect, it } from "vitest";

import {
  buildChimeId,
  getWavDurationMs,
  MAX_CHIME_DURATION_MS,
  MAX_CHIME_UPLOAD_BYTES,
} from "./paging-chime-upload.js";

describe("paging-chime-upload", () => {
  it("builds slugged chime ids", () => {
    const chimeId = buildChimeId("Front Door Bell");
    expect(chimeId.startsWith("front-door-bell-")).toBe(true);
  });

  it("parses wav duration from a generated buffer", () => {
    const sampleRate = 22050;
    const durationSec = 0.2;
    const samples = Math.floor(sampleRate * durationSec);
    const dataSize = samples * 2;
    const buffer = Buffer.alloc(44 + dataSize);
    buffer.write("RIFF", 0);
    buffer.writeUInt32LE(36 + dataSize, 4);
    buffer.write("WAVE", 8);
    buffer.write("fmt ", 12);
    buffer.writeUInt32LE(16, 16);
    buffer.writeUInt16LE(1, 20);
    buffer.writeUInt16LE(1, 22);
    buffer.writeUInt32LE(sampleRate, 24);
    buffer.writeUInt32LE(sampleRate * 2, 28);
    buffer.writeUInt16LE(2, 32);
    buffer.writeUInt16LE(16, 34);
    buffer.write("data", 36);
    buffer.writeUInt32LE(dataSize, 40);

    const durationMs = getWavDurationMs(buffer);
    expect(durationMs).toBeGreaterThan(0);
    expect(durationMs).toBeLessThanOrEqual(MAX_CHIME_DURATION_MS);
  });

  it("exports upload limits", () => {
    expect(MAX_CHIME_UPLOAD_BYTES).toBe(512_000);
    expect(MAX_CHIME_DURATION_MS).toBe(3000);
  });
});
