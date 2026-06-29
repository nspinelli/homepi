import { describe, expect, it } from "vitest";

import { createRequest, createSuccessResponse, isUiVisibleEvent } from "./index.js";

describe("core-messaging", () => {
  it("creates v1 request envelopes", () => {
    const request = createRequest({
      source: "homepi-backend",
      target: "homepi-health",
      command: "health.snapshot",
    });

    expect(request.v).toBe(1);
    expect(request.command).toBe("health.snapshot");
  });

  it("filters transport events from UI log", () => {
    expect(isUiVisibleEvent({ event: "heartbeat" })).toBe(false);
    expect(isUiVisibleEvent({ topic: "homepi.audio.zone.changed", uiVisible: true })).toBe(true);
  });

  it("creates success responses", () => {
    const request = createRequest({
      source: "a",
      target: "b",
      command: "ping",
    });
    const response = createSuccessResponse(request, { pong: true });
    expect(response.ok).toBe(true);
  });
});
