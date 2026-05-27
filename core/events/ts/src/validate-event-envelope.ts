import { createSchemaValidator } from "@homepi/tooling-schema-validation";
import type { SchemaValidationResult } from "@homepi/tooling-schema-validation";
import eventEnvelopeSchema from "../../schema/event-envelope.schema.json" with { type: "json" };
import type { EventEnvelope } from "./event-types.js";

const eventEnvelopeValidator = createSchemaValidator<EventEnvelope>(eventEnvelopeSchema);

/**
 * Validates a value against event-envelope.schema.json.
 * @param envelope - Candidate event envelope.
 * @returns Validation result.
 */
export function validateEventEnvelope(envelope: unknown): SchemaValidationResult & {
  value?: EventEnvelope;
} {
  return eventEnvelopeValidator.validate(envelope);
}
