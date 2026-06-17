import { describe, expect, it } from "vitest";

import {
  buildSourceSettingsPatch,
  hasSourceSettingsChanges,
  sourceToFormState,
} from "@/components/audio/source-settings-patch.js";
import type { HifiSource } from "@/types/audio-types.js";

const baseSource: HifiSource = {
  sourceNumber: 3,
  name: "Radio",
  enabled: 1,
  inputGain: 5,
  displayLine: "FM",
  isAirplay: 0,
};

describe("source-settings-patch", () => {
  it("detects controller field changes", () => {
    const form = sourceToFormState(baseSource);
    form.name = "Satellite";
    form.inputGain = 8;

    expect(hasSourceSettingsChanges(baseSource, form)).toBe(true);
    expect(buildSourceSettingsPatch(baseSource, form)).toEqual({
      controller: {
        name: "Satellite",
        inputGain: 8,
      },
    });
  });

  it("includes airplay when designated", () => {
    const form = sourceToFormState(baseSource);
    form.isAirplay = true;

    expect(buildSourceSettingsPatch(baseSource, form)).toEqual({
      airplay: true,
    });
  });

  it("returns null when nothing changed", () => {
    const form = sourceToFormState(baseSource);
    expect(buildSourceSettingsPatch(baseSource, form)).toBeNull();
  });
});
