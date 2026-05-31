import { describe, expect, it } from "vitest";

import { normalizeJournalLogLine } from "./journal-log-bridge.js";

describe("normalizeJournalLogLine", () => {
  it("accepts core logging shape", () => {
    const log = normalizeJournalLogLine({
      ts: "2026-05-31T19:00:00Z",
      service: "homepi-pcm-router",
      module: "main",
      level: "INFO",
      event: "started",
      correlationId: "started",
      message: "ok",
      data: {},
    });
    expect(log?.service).toBe("homepi-pcm-router");
    expect(log?.level).toBe("INFO");
  });

  it("normalizes legacy timestamp and lowercase level", () => {
    const log = normalizeJournalLogLine({
      timestamp: "2026-05-31T18:27:31.000Z",
      service: "homepi-pcm-router",
      module: "zone_state",
      level: "info",
      event: "route_start",
      message: "zone=8",
    });
    expect(log?.ts).toBe("2026-05-31T18:27:31.000Z");
    expect(log?.level).toBe("INFO");
    expect(log?.correlationId).toBe("route_start");
  });
});
