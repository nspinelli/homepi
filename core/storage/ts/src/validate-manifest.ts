import { createSchemaValidator } from "@homepi/tooling-schema-validation";
import type { SchemaValidationResult } from "@homepi/tooling-schema-validation";
import databaseManifestSchema from "../../schema/database-manifest.schema.json" with { type: "json" };
import migrationManifestSchema from "../../schema/migration-manifest.schema.json" with { type: "json" };
import type { DatabaseManifest, ManifestKind, MigrationManifest } from "./storage-types.js";

const migrationValidator = createSchemaValidator<MigrationManifest>(migrationManifestSchema);
const databaseValidator = createSchemaValidator<DatabaseManifest>(databaseManifestSchema);

/**
 * Validates a manifest against the documented storage schemas.
 * @param kind - Manifest schema kind.
 * @param manifest - Manifest value to validate.
 * @returns Validation result.
 */
export function validateManifest(
  kind: "migration",
  manifest: unknown
): SchemaValidationResult & { value?: MigrationManifest };
export function validateManifest(
  kind: "database",
  manifest: unknown
): SchemaValidationResult & { value?: DatabaseManifest };
export function validateManifest(
  kind: ManifestKind,
  manifest: unknown
): SchemaValidationResult & { value?: MigrationManifest | DatabaseManifest } {
  if (kind === "migration") {
    return migrationValidator.validate(manifest);
  }
  return databaseValidator.validate(manifest);
}
