import { describe, expect, it } from "vitest";

import {
  isBrokerAudioSnapshotEnabled,
  isBrokerOnlyAudioSseEnabled,
} from "./audio-ui-bridge.js";

describe("audio-ui-bridge", () => {
  it("defaults broker-only audio SSE to enabled", () => {
    delete process.env.HOMEPI_BROKER_ONLY_AUDIO_SSE;
    expect(isBrokerOnlyAudioSseEnabled()).toBe(true);
  });

  it("allows disabling broker-only audio SSE via env", () => {
    process.env.HOMEPI_BROKER_ONLY_AUDIO_SSE = "false";
    expect(isBrokerOnlyAudioSseEnabled()).toBe(false);
    delete process.env.HOMEPI_BROKER_ONLY_AUDIO_SSE;
  });

  it("defaults broker audio snapshot cache to enabled", () => {
    delete process.env.HOMEPI_BROKER_AUDIO_SNAPSHOT;
    expect(isBrokerAudioSnapshotEnabled()).toBe(true);
  });
});
