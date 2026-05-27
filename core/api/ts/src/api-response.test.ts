import { describe, expect, it } from "vitest";
import apiErrorExample from "../../examples/api-error.example.json" with { type: "json" };
import apiResponseExample from "../../examples/api-response.example.json" with { type: "json" };
import apiErrorSchema from "../../schema/api-error.schema.json" with { type: "json" };
import apiResponseSchema from "../../schema/api-response.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { createSuccessResponse } from "./create-api-response.js";

describe("ApiResponse", () => {
  it("validates the documented examples", () => {
    const responseResult = validateAgainstSchema(apiResponseSchema, apiResponseExample, [
      apiErrorSchema,
    ]);
    const errorResult = validateAgainstSchema(apiErrorSchema, apiErrorExample.error);
    expect(responseResult.valid).toBe(true);
    expect(errorResult.valid).toBe(true);
  });

  it("creates schema-valid success responses", () => {
    const response = createSuccessResponse({
      correlationId: "api-001",
      data: { status: "healthy" },
      timestamp: "2026-05-27T16:00:00.000Z",
    });
    const result = validateAgainstSchema(apiResponseSchema, response, [apiErrorSchema]);
    expect(result.valid).toBe(true);
  });
});
