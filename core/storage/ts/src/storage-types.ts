/**
 * Supported database engine per database-manifest.schema.json.
 */
export type DatabaseEngine = "sqlite";

/**
 * Database manifest shape per database-manifest.schema.json.
 */
export interface DatabaseManifest {
  /** Database name. */
  name: string;
  /** Storage engine. */
  engine: DatabaseEngine;
  /** Absolute database path. */
  path: string;
  /** Owning service. */
  owner: string;
}

/**
 * Migration manifest shape per migration-manifest.schema.json.
 */
export interface MigrationManifest {
  /** Migration identifier. */
  id: string;
  /** Target database name. */
  database: string;
  /** ISO8601 UTC creation timestamp. */
  createdAt: string;
  /** Human-readable description. */
  description: string;
}

/**
 * Manifest kind discriminator for validation.
 */
export type ManifestKind = "migration" | "database";
