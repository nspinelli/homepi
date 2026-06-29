import { describe, expect, it } from "vitest";

import { brokerEventToEventEnvelope, parseBrokerWireLine } from "./broker-event-adapter.js";

describe("broker-event-adapter", () => {
  it("maps broker events to legacy SSE envelopes", () => {
    const envelope = brokerEventToEventEnvelope({
      v: 1,
      id: "evt-1",
      ts: "2026-06-28T12:00:00.000Z",
      topic: "modules.metadata.snapshot",
      source: "homepi-metadata",
      correlationId: "corr-1",
      severity: "info",
      payload: {
        event: "metadata_snapshot",
        ownerZoneId: 2,
      },
    });

    expect(envelope).toEqual({
      version: 1,
      id: "evt-1",
      source: "homepi-metadata",
      topic: "modules.metadata.snapshot",
      event: "metadata_snapshot",
      correlationId: "corr-1",
      timestamp: "2026-06-28T12:00:00.000Z",
      payload: {
        ownerZoneId: 2,
      },
    });
  });

  it("parses broker wire lines", () => {
    const envelope = parseBrokerWireLine(
      JSON.stringify({
        type: "event",
        event: {
          v: 1,
          id: "evt-2",
          ts: "2026-06-28T12:00:01.000Z",
          topic: "modules.pcm.routing",
          source: "homepi-pcm-router",
          correlationId: "corr-2",
          severity: "info",
          payload: {
            event: "owner_changed",
            ownerZoneId: 3,
          },
        },
      })
    );

    expect(envelope?.event).toBe("owner_changed");
    expect(envelope?.payload).toEqual({ ownerZoneId: 3 });
  });
});
