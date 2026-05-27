import { describe, expect, it } from "vitest";
import eventEnvelopeExample from "../../examples/event-envelope.example.json" with { type: "json" };
import eventEnvelopeSchema from "../../schema/event-envelope.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { createEventEnvelope } from "./event-envelope.js";

describe("EventEnvelope", () => {
  it("validates the documented example", () => {
    const result = validateAgainstSchema(eventEnvelopeSchema, eventEnvelopeExample);
    expect(result.valid).toBe(true);
  });

  it("creates schema-valid envelopes", () => {
    const envelope = createEventEnvelope({
      source: "homepi-hifi-serial",
      topic: "modules.audio.zone",
      event: "zone_power_changed",
      correlationId: "cmd-001",
      payload: { zone: 4, power: true },
    });
    const result = validateAgainstSchema(eventEnvelopeSchema, envelope);
    expect(result.valid).toBe(true);
  });
});
