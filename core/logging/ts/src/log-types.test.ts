import { describe, expect, it } from "vitest";
import logMessageSchema from "../../schema/log-message.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { createLogger } from "./logger.js";

describe("LogMessage schema", () => {
  it("validates a built log message", () => {
    const logger = createLogger({ service: "homepi-backend", minLevel: "DEBUG" });
    const message = logger.buildMessage("INFO", {
      module: "core.logging",
      event: "service_started",
      correlationId: "startup-001",
      message: "Service started",
      data: {},
    });

    const result = validateAgainstSchema(logMessageSchema, message);
    expect(result.valid).toBe(true);
  });
});
