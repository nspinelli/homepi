import { describe, expect, it } from "vitest";
import transportEnvelopeSchema from "../../schema/transport-envelope.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import transportEnvelopeExample from "../../examples/transport-envelope.example.json" with { type: "json" };
import { createTransportEnvelope } from "./envelope.js";

describe("TransportEnvelope", () => {
  it("validates the documented example", () => {
    const result = validateAgainstSchema(transportEnvelopeSchema, transportEnvelopeExample);
    expect(result.valid).toBe(true);
  });

  it("creates schema-valid envelopes", () => {
    const envelope = createTransportEnvelope({
      type: "event",
      source: "homepi-backend",
      topic: "system.health",
      correlationId: "trace-001",
      payload: { ok: true },
    });
    const result = validateAgainstSchema(transportEnvelopeSchema, envelope);
    expect(result.valid).toBe(true);
  });
});
