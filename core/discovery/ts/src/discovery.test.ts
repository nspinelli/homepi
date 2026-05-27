import { describe, expect, it } from "vitest";
import serviceDiscoveryExample from "../../examples/service-discovery.example.json" with { type: "json" };
import serviceDiscoverySchema from "../../schema/service-discovery.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { createServiceRecord } from "./create-service-record.js";

describe("ServiceDiscovery", () => {
  it("validates the documented example", () => {
    const result = validateAgainstSchema(serviceDiscoverySchema, serviceDiscoveryExample);
    expect(result.valid).toBe(true);
  });

  it("creates schema-valid service records", () => {
    const record = createServiceRecord({
      service: "homepi-hifi-serial",
      host: "homepi.local",
      socketPath: "/run/homepi/hifi-serial.sock",
      capabilities: ["audio.hifi.serial"],
    });
    const result = validateAgainstSchema(serviceDiscoverySchema, record);
    expect(result.valid).toBe(true);
  });
});
