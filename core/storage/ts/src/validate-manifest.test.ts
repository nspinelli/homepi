import { describe, expect, it } from "vitest";
import migrationManifestExample from "../../examples/migration-manifest.example.json" with { type: "json" };
import migrationManifestSchema from "../../schema/migration-manifest.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { validateManifest } from "./validate-manifest.js";

describe("MigrationManifest", () => {
  it("validates the documented example", () => {
    const result = validateAgainstSchema(migrationManifestSchema, migrationManifestExample);
    expect(result.valid).toBe(true);
  });

  it("validates through validateManifest helper", () => {
    const result = validateManifest("migration", migrationManifestExample);
    expect(result.valid).toBe(true);
  });
});
