import { createSchemaValidator } from "@homepi/tooling-schema-validation";
import type { SchemaValidationResult } from "@homepi/tooling-schema-validation";
import serviceConfigSchema from "../../schema/service-config.schema.json" with { type: "json" };
import runtimeConfigSchema from "../../schema/runtime-config.schema.json" with { type: "json" };
import type { RuntimeConfig, ServiceConfig } from "./config-types.js";

const serviceValidator = createSchemaValidator<ServiceConfig>(serviceConfigSchema, [
  runtimeConfigSchema,
]);
const runtimeValidator = createSchemaValidator<RuntimeConfig>(runtimeConfigSchema);

/**
 * Validates a service configuration object against schema.
 * @param config - Configuration to validate.
 * @returns Validation result.
 */
export function validateServiceConfig(config: unknown): SchemaValidationResult & {
  value?: ServiceConfig;
} {
  return serviceValidator.validate(config);
}

/**
 * Validates runtime configuration against schema.
 * @param config - Runtime configuration.
 * @returns Validation result.
 */
export function validateRuntimeConfig(config: unknown): SchemaValidationResult & {
  value?: RuntimeConfig;
} {
  return runtimeValidator.validate(config);
}
