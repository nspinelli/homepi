import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import type { ModuleRegistryEntry, ServiceRegistry, ServiceRegistryEntry } from "./registry-types.js";

const __dirname = dirname(fileURLToPath(import.meta.url));

/**
 * Default path to the bundled registry JSON relative to the package.
 */
export const DEFAULT_REGISTRY_PATH = join(__dirname, "..", "registry.json");

/**
 * Loads and parses the HomePi service registry from disk.
 * @param registryPath - Optional override path to registry.json.
 * @returns Parsed service registry.
 */
export function loadServiceRegistry(registryPath: string = DEFAULT_REGISTRY_PATH): ServiceRegistry {
  const raw = readFileSync(registryPath, "utf8");
  const parsed = JSON.parse(raw) as ServiceRegistry;

  if (parsed.version !== 1 || !Array.isArray(parsed.modules) || !Array.isArray(parsed.services)) {
    throw new Error(`Invalid service registry at ${registryPath}`);
  }

  return parsed;
}

/**
 * Finds a module entry by id.
 * @param registry - Loaded registry.
 * @param moduleId - Module key.
 * @returns Module entry or undefined.
 */
export function findModule(
  registry: ServiceRegistry,
  moduleId: string
): ModuleRegistryEntry | undefined {
  return registry.modules.find((entry) => entry.id === moduleId);
}

/**
 * Finds a service entry by name.
 * @param registry - Loaded registry.
 * @param serviceName - Service name.
 * @returns Service entry or undefined.
 */
export function findService(
  registry: ServiceRegistry,
  serviceName: string
): ServiceRegistryEntry | undefined {
  return registry.services.find((entry) => entry.name === serviceName);
}

/**
 * Returns services belonging to a module.
 * @param registry - Loaded registry.
 * @param moduleId - Module key or `platform`.
 * @returns Matching service entries.
 */
export function servicesForModule(
  registry: ServiceRegistry,
  moduleId: string
): ServiceRegistryEntry[] {
  return registry.services.filter((entry) => entry.module === moduleId);
}
