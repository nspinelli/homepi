import type { RuntimeConfig, RuntimePaths, ServiceConfig } from "./config-types.js";

const DEFAULT_PATHS: RuntimePaths = {
  homepiRoot: "/opt/homepi",
  runtimeDir: "/opt/homepi/runtime",
  generatedDir: "/opt/homepi/runtime/generated",
  cacheDir: "/opt/homepi/runtime/cache",
  stateDir: "/opt/homepi/runtime/state",
  socketDir: "/run/homepi",
};

/**
 * Normalizes runtime paths with documented defaults.
 * @param runtime - Partial runtime configuration.
 * @returns Normalized runtime configuration.
 */
export function normalizeRuntimeConfig(runtime: Partial<RuntimeConfig>): RuntimeConfig {
  return {
    paths: {
      ...DEFAULT_PATHS,
      ...runtime.paths,
    },
    watchdog: runtime.watchdog,
    service: runtime.service,
  };
}

/**
 * Normalizes a validated service configuration.
 * @param config - Validated service config.
 * @returns Normalized service config.
 */
export function normalizeServiceConfig(config: ServiceConfig): ServiceConfig {
  return {
    ...config,
    enabled: config.enabled ?? true,
    runtime: normalizeRuntimeConfig(config.runtime),
    logging: {
      ...config.logging,
      debugTrace: config.logging.debugTrace ?? false,
    },
  };
}
