import type { ServiceConfig } from "@homepi/core-config";

import { FALLBACK_RECONCILIATION_INTERVAL_MS } from "./fallback-reconciliation.js";

/**
 * Resolved fallback reconciliation settings from service config.
 */
export interface ResolvedFallbackReconciliation {
  /** Whether the reconciliation loop should run. */
  enabled: boolean;
  /** Interval in milliseconds when enabled. */
  intervalMs: number;
}

/**
 * Resolves fallback reconciliation settings from loaded service config.
 * @param config - Loaded backend service configuration.
 * @returns Resolved enabled flag and interval.
 */
export function resolveFallbackReconciliation(
  config: ServiceConfig
): ResolvedFallbackReconciliation {
  const fallback = config.status?.fallbackReconciliation;
  const enabled = fallback?.enabled !== false;
  const intervalMs = fallback?.intervalMs ?? FALLBACK_RECONCILIATION_INTERVAL_MS;
  return { enabled, intervalMs };
}
