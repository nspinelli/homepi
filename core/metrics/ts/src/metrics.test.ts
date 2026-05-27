import { describe, expect, it } from "vitest";
import metricSampleExample from "../../examples/metric-sample.example.json" with { type: "json" };
import metricSampleSchema from "../../schema/metric-sample.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { Counter } from "./counter.js";
import { createMetricSample } from "./create-metric-sample.js";

describe("MetricSample", () => {
  it("validates the documented example", () => {
    const result = validateAgainstSchema(metricSampleSchema, metricSampleExample);
    expect(result.valid).toBe(true);
  });

  it("creates schema-valid metric samples", () => {
    const sample = createMetricSample({
      name: "transport.messages_sent",
      type: "counter",
      value: 42,
      timestamp: "2026-05-27T16:00:00.000Z",
      labels: { service: "homepi-backend" },
    });
    const result = validateAgainstSchema(metricSampleSchema, sample);
    expect(result.valid).toBe(true);
  });

  it("samples counter values", () => {
    const counter = new Counter("transport.messages_sent", { service: "homepi-backend" });
    counter.increment(42);
    const result = validateAgainstSchema(metricSampleSchema, counter.sample());
    expect(result.valid).toBe(true);
    expect(counter.sample().value).toBe(42);
  });
});
