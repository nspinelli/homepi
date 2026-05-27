import type { Permission } from "./auth-types.js";

/**
 * Determines whether permissions grant an action on a resource.
 * Explicit deny entries take precedence over allow entries.
 * @param permissions - Permission list to evaluate.
 * @param resource - Resource identifier.
 * @param action - Action name.
 * @returns True when access is allowed.
 */
export function hasPermission(
  permissions: Permission[],
  resource: string,
  action: string
): boolean {
  const matches = permissions.filter(
    (permission) => permission.resource === resource && permission.action === action
  );

  if (matches.some((permission) => permission.effect === "deny")) {
    return false;
  }

  return matches.some(
    (permission) => permission.effect === "allow" || permission.effect === undefined
  );
}
