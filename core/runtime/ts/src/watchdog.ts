import type { WatchdogStatus } from "./runtime-types.js";

/**
 * Creates a watchdog status snapshot.
 * @param service - Service name.
 * @param enabled - Whether watchdog is enabled.
 * @param healthy - Current health flag.
 * @param timeoutMs - Watchdog timeout.
 * @returns Watchdog status object.
 */
export function createWatchdogStatus(
  service: string,
  enabled: boolean,
  healthy: boolean,
  timeoutMs?: number
): WatchdogStatus {
  return {
    service,
    enabled,
    healthy,
    timeoutMs,
    lastPingAt: healthy ? new Date().toISOString() : undefined,
  };
}
