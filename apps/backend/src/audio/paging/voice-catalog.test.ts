import { describe, expect, it } from "vitest";

import {
  buildPiperVoiceSampleUrl,
  isEnglishVoice,
  parsePiperVoiceId,
  resolvePagingVoiceFilePaths,
} from "./voice-catalog.js";

describe("voice-catalog", () => {
  it("parses Piper voice ids", () => {
    expect(parsePiperVoiceId("en_US-lessac-medium")).toEqual({
      lang: "en",
      region: "US",
      speaker: "lessac",
      quality: "medium",
    });
    expect(parsePiperVoiceId("en_GB-jenny_dioco-medium")).toEqual({
      lang: "en",
      region: "GB",
      speaker: "jenny_dioco",
      quality: "medium",
    });
  });

  it("builds Hugging Face download URLs", () => {
    const paths = resolvePagingVoiceFilePaths("en_US-amy-low");
    expect(paths.modelUrl).toContain("en/en_US/amy/low/en_US-amy-low.onnx");
    expect(paths.configUrl).toContain("en_US-amy-low.onnx.json");
    expect(paths.modelPath).toContain("/var/lib/homepi/paging/voices/en_US-amy-low.onnx");
  });

  it("builds Hugging Face sample URLs", () => {
    expect(buildPiperVoiceSampleUrl("en_US-lessac-medium")).toBe(
      "https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/samples/speaker_0.mp3"
    );
    expect(buildPiperVoiceSampleUrl("en_GB-alan-medium")).toContain(
      "/en/en_GB/alan/medium/samples/speaker_0.mp3"
    );
    expect(buildPiperVoiceSampleUrl("invalid-voice")).toBeNull();
  });

  it("filters English catalog voices", () => {
    expect(
      isEnglishVoice({
        voiceId: "en_US-lessac-medium",
        displayName: "Lessac",
        languageCode: "en-US",
        accent: "US English",
        quality: "medium",
        isBundled: true,
        sampleAvailable: true,
      })
    ).toBe(true);
    expect(
      isEnglishVoice({
        voiceId: "de_DE-thorsten-medium",
        displayName: "Thorsten",
        languageCode: "de-DE",
        accent: "German",
        quality: "medium",
        isBundled: false,
        sampleAvailable: false,
      })
    ).toBe(false);
  });
});
