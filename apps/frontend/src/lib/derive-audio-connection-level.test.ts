import { describe, expect, it } from "vitest";

import {
  deriveAudioConnectionLevel,
  deriveAudioConnectionLevelFromSnapshot,
} from "./derive-audio-connection-level.js";
import type { AudioSnapshot } from "@/types/audio-types.js";

const placeholderServices: AudioSnapshot["services"] = {
  hifiSerial: "offline",
  shairport: "offline",
  pcmRouter: "offline",
  nqptp: "offline",
  metadata: "offline",
};

describe("deriveAudioConnectionLevel", () => {
  it("returns healthy before REST service rows hydrate when the Hi-Fi link is up", () => {
    expect(
      deriveAudioConnectionLevel(true, placeholderServices, false, false)
    ).toBe("healthy");
  });

  it("returns offline when hydrated services include a failed row", () => {
    expect(
      deriveAudioConnectionLevel(
        true,
        { ...placeholderServices, metadata: "failed" },
        false,
        true
      )
    ).toBe("offline");
  });

  it("ignores placeholder offline rows when SSE shows an active PCM route", () => {
    expect(
      deriveAudioConnectionLevelFromSnapshot(
        {
          hifiConnected: true,
          controller: { serialPath: "/dev/vHifi" },
          services: placeholderServices,
          pcm: { ownerZoneId: 8, activeStack: [8] },
        } as AudioSnapshot,
        { servicesHydrated: true }
      )
    ).toBe("healthy");
  });
});
