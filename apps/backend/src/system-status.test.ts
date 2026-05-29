import { describe, expect, it } from "vitest";
import apiResponseSchema from "../../../core/api/schema/api-response.schema.json" with { type: "json" };
import apiErrorSchema from "../../../core/api/schema/api-error.schema.json" with { type: "json" };
import healthReportSchema from "../../../core/health/schema/health-report.schema.json" with { type: "json" };
import healthCheckSchema from "../../../core/health/schema/health-check.schema.json" with { type: "json" };
import runtimeStatusSchema from "../../../core/runtime/schema/runtime-status.schema.json" with { type: "json" };
import eventEnvelopeSchema from "../../../core/events/schema/event-envelope.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { createSuccessResponse } from "@homepi/core-api";
import { createHealthReport } from "@homepi/core-health";
import { createEventEnvelope } from "@homepi/core-events";
import coreStatusExample from "../examples/core-status-response.example.json" with { type: "json" };
import runtimeStatusExample from "../examples/runtime-status-response.example.json" with { type: "json" };
import sseEventExample from "../examples/sse-system-status-event.example.json" with { type: "json" };
import { buildCoreStatusPayload } from "./core-status-builder.js";
import { buildRuntimeStatusPayload } from "./runtime-status-builder.js";
import { SystemStatusStore } from "./system-status-store.js";
import type { ServiceConfig } from "@homepi/core-config";

const testConfig: ServiceConfig = {
  service: "homepi-backend",
  environment: "test",
  logging: { level: "INFO" },
  runtime: {
    paths: {
      homepiRoot: "/opt/homepi",
      runtimeDir: "/opt/homepi/runtime",
      generatedDir: "/opt/homepi/runtime/generated",
      stateDir: "/opt/homepi/runtime/state",
      socketDir: "/run/homepi",
    },
  },
};

describe("system status vertical slice", () => {
  it("builds schema-valid runtime status payloads", () => {
    const startedAt = new Date("2026-05-27T15:58:00.000Z");
    const payload = buildRuntimeStatusPayload(testConfig, startedAt);
    const result = validateAgainstSchema(runtimeStatusSchema, payload);
    expect(result.valid).toBe(true);
  });

  it("builds API envelopes with correlationId for health and status", () => {
    const correlationId = "test-correlation";
    const healthReport = createHealthReport({
      service: testConfig.service,
      checks: [{ name: "http", status: "pass" }],
      checkedAt: "2026-05-27T16:00:00.000Z",
    });

    const healthResponse = createSuccessResponse({
      correlationId,
      data: healthReport as unknown as Record<string, unknown>,
    });

    expect(healthResponse.correlationId).toBe(correlationId);
    expect(healthResponse.ok).toBe(true);

    const healthValidation = validateAgainstSchema(apiResponseSchema, healthResponse, [
      apiErrorSchema,
    ]);
    const reportValidation = validateAgainstSchema(healthReportSchema, healthReport, [
      healthCheckSchema,
    ]);
    expect(healthValidation.valid).toBe(true);
    expect(reportValidation.valid).toBe(true);
  });

  it("validates documented core status and runtime examples", () => {
    const coreResult = validateAgainstSchema(apiResponseSchema, coreStatusExample, [
      apiErrorSchema,
    ]);
    const runtimeResult = validateAgainstSchema(apiResponseSchema, runtimeStatusExample, [
      apiErrorSchema,
    ]);
    expect(coreResult.valid).toBe(true);
    expect(runtimeResult.valid).toBe(true);
    expect(runtimeStatusExample.data).toBeDefined();
    const runtimeDataResult = validateAgainstSchema(
      runtimeStatusSchema,
      runtimeStatusExample.data
    );
    expect(runtimeDataResult.valid).toBe(true);
  });

  it("creates schema-valid SSE system status envelopes", () => {
    const store = new SystemStatusStore({
      backend: "healthy",
      config: "loaded",
      logging: "active",
      runtime: "running",
      transport: "ready",
      events: "ready",
      state: "ready",
      api: "ready",
      usbDevices: "offline",
      hifiSerial: "offline",
      nqptp: "offline",
      metadata: "offline",
      pcmRouter: "offline",
      uptimeMs: 0,
      lastEventAt: null,
    });

    const envelope = createEventEnvelope({
      source: "homepi-backend",
      topic: "system.status",
      event: "system_status_snapshot",
      correlationId: "evt-test",
      payload: { snapshot: store.getStatus() },
    });

    const result = validateAgainstSchema(eventEnvelopeSchema, envelope);
    expect(result.valid).toBe(true);

    const exampleResult = validateAgainstSchema(eventEnvelopeSchema, sseEventExample);
    expect(exampleResult.valid).toBe(true);
  });

  it("aggregates core status from the state store", () => {
    const store = new SystemStatusStore({
      backend: "healthy",
      config: "loaded",
      logging: "active",
      runtime: "running",
      transport: "ready",
      events: "ready",
      state: "ready",
      api: "ready",
      usbDevices: "healthy",
      hifiSerial: "healthy",
      nqptp: "healthy",
      metadata: "healthy",
      pcmRouter: "healthy",
      uptimeMs: 42,
      lastEventAt: "2026-05-27T16:00:00.000Z",
    });

    const payload = buildCoreStatusPayload(testConfig, store.getStatus());
    expect(payload.services).toHaveLength(12);
    expect(payload.system.uptimeMs).toBe(42);
  });
});
