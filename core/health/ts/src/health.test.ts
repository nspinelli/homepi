import { describe, expect, it } from "vitest";
import healthReportExample from "../../examples/health-report.example.json" with { type: "json" };
import healthCheckSchema from "../../schema/health-check.schema.json" with { type: "json" };
import healthReportSchema from "../../schema/health-report.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { aggregateHealth } from "./aggregate-health.js";
import { createHealthReport } from "./create-health-report.js";
import type { HealthReport } from "./health-types.js";

describe("HealthReport", () => {
  it("validates the documented example", () => {
    const report = healthReportExample as HealthReport;
    const reportResult = validateAgainstSchema(healthReportSchema, report, [
      healthCheckSchema,
    ]);
    const checkResults = report.checks.map((check) =>
      validateAgainstSchema(healthCheckSchema, check)
    );
    expect(reportResult.valid).toBe(true);
    expect(checkResults.every((result) => result.valid)).toBe(true);
  });

  it("aggregates check statuses", () => {
    expect(aggregateHealth([{ name: "database", status: "pass" }])).toBe("healthy");
    expect(aggregateHealth([{ name: "cache", status: "warn" }])).toBe("degraded");
    expect(aggregateHealth([{ name: "socket", status: "fail" }])).toBe("failed");
  });

  it("creates schema-valid health reports", () => {
    const report = createHealthReport({
      service: "homepi-backend",
      checks: [{ name: "database", status: "pass", durationMs: 2 }],
      checkedAt: "2026-05-27T16:00:00.000Z",
    });
    const result = validateAgainstSchema(healthReportSchema, report, [healthCheckSchema]);
    expect(result.valid).toBe(true);
  });
});
