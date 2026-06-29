import { describe, expect, it } from "vitest";

import { isUiVisibleEvent } from "./activity-log-filter.js";

describe("activity-log-filter", () => {
  it("excludes heartbeat events", () => {
    expect(isUiVisibleEvent({ event: "heartbeat", payload: {} })).toBe(false);
  });

  it("excludes audio realtime progress events", () => {
    expect(
      isUiVisibleEvent({
        event: "audio.realtime",
        topic: "modules.audio.realtime",
        payload: {},
      })
    ).toBe(false);
  });

  it("excludes events marked uiVisible false", () => {
    expect(
      isUiVisibleEvent({
        event: "zone_power_changed",
        payload: { uiVisible: false },
      })
    ).toBe(false);
  });

  it("excludes debug log records", () => {
    expect(
      isUiVisibleEvent({
        event: "log_record",
        payload: { level: "DEBUG", message: "verbose detail" },
      })
    ).toBe(false);
  });

  it("includes domain events", () => {
    expect(
      isUiVisibleEvent({
        event: "contact_changed",
        topic: "homepi.sensors.contact.changed",
        payload: {},
      })
    ).toBe(true);
  });
});
