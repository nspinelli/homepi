import type { RuntimeConfig } from "./config-types.js";

/**
 * Deep-merges runtime configuration layers; later layers override earlier.
 * @param layers - Runtime config layers in precedence order.
 * @returns Merged runtime configuration.
 */
export function mergeRuntimeConfig(...layers: Partial<RuntimeConfig>[]): RuntimeConfig {
  const result: RuntimeConfig = { paths: { homepiRoot: "", runtimeDir: "", generatedDir: "", socketDir: "" } };

  for (const layer of layers) {
    if (layer.paths) {
      result.paths = { ...result.paths, ...layer.paths };
    }
    if (layer.watchdog) {
      result.watchdog = { ...result.watchdog, ...layer.watchdog };
    }
    if (layer.service) {
      result.service = { ...result.service, ...layer.service };
    }
  }

  return result;
}
