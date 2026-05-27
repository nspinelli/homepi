/**
 * Principal type per principal.schema.json.
 */
export type PrincipalType = "user" | "service" | "system";

/**
 * Principal shape per principal.schema.json.
 */
export interface Principal {
  /** Stable principal identifier. */
  id: string;
  /** Principal category. */
  type: PrincipalType;
  /** Human-readable display name. */
  displayName: string;
  /** Assigned roles. */
  roles?: string[];
}

/**
 * Session shape per session.schema.json.
 */
export interface Session {
  /** Session identifier. */
  sessionId: string;
  /** Associated principal identifier. */
  principalId: string;
  /** ISO8601 UTC creation timestamp. */
  createdAt: string;
  /** ISO8601 UTC expiration timestamp. */
  expiresAt: string;
}

/**
 * Permission effect per permission.schema.json.
 */
export type PermissionEffect = "allow" | "deny";

/**
 * Permission shape per permission.schema.json.
 */
export interface Permission {
  /** Protected resource identifier. */
  resource: string;
  /** Allowed action name. */
  action: string;
  /** Permission effect; defaults to allow. */
  effect?: PermissionEffect;
}
