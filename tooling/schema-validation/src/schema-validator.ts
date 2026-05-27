import AjvModule, { type ErrorObject, type ValidateFunction } from "ajv";
import addFormatsModule from "ajv-formats";

const Ajv = AjvModule.default ?? AjvModule;
const addFormats = addFormatsModule.default ?? addFormatsModule;

/**
 * Result of validating a value against a JSON Schema.
 */
export interface SchemaValidationResult {
  /** Whether validation succeeded. */
  valid: boolean;
  /** AJV error objects when validation failed. */
  errors?: ErrorObject[];
}

/**
 * Compiled schema validator instance.
 */
export interface SchemaValidator<T = unknown> {
  /** Validate a value; returns structured result. */
  validate(value: unknown): SchemaValidationResult & { value?: T };
  /** Underlying AJV validate function. */
  readonly fn: ValidateFunction<T>;
}

/**
 * Creates a reusable JSON Schema validator with draft 2020-12 support.
 * @param schema - JSON Schema document.
 * @param refs - Additional schemas registered before compile (for $ref resolution).
 * @returns Compiled validator.
 */
export function createSchemaValidator<T = unknown>(
  schema: object,
  refs: object[] = []
): SchemaValidator<T> {
  const ajv = new Ajv({
    allErrors: true,
    strict: false,
    validateSchema: false,
  });
  addFormats(ajv);
  for (const refSchema of refs) {
    ajv.addSchema(refSchema);
  }
  const fn = ajv.compile<T>(schema);

  return {
    fn,
    validate(value: unknown): SchemaValidationResult & { value?: T } {
      const valid = fn(value);
      if (valid) {
        return { valid: true, value: value as T };
      }
      return { valid: false, errors: fn.errors ?? undefined };
    },
  };
}

/**
 * Validates a value against a JSON Schema in one shot.
 * @param schema - JSON Schema document.
 * @param value - Value to validate.
 * @returns Validation result.
 */
export function validateAgainstSchema<T = unknown>(
  schema: object,
  value: unknown,
  refs: object[] = []
): SchemaValidationResult & { value?: T } {
  return createSchemaValidator<T>(schema, refs).validate(value);
}
