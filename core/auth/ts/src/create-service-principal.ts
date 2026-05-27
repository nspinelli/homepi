import type { Principal } from "./auth-types.js";

/**
 * Creates a service principal record.
 * @param params - Principal fields.
 * @returns Service principal.
 */
export function createServicePrincipal(params: {
  id: string;
  displayName: string;
  roles?: string[];
}): Principal {
  return {
    id: params.id,
    type: "service",
    displayName: params.displayName,
    roles: params.roles,
  };
}
