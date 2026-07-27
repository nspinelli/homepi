import type { SensorsSnapshot } from "@/types/sensors-types.js";

/** Aggregate contact sensors connection health for dashboard display. */
export type SensorsConnectionLevel = "healthy" | "degraded" | "offline";

/**
 * Options for deriving dashboard connection health.
 */
export interface DeriveSensorsConnectionOptions {
  /** When false, service rows from REST are not trusted yet. */
  servicesHydrated?: boolean;
}

/**
 * Human-readable label for a connection level pill.
 * @param level - Connection level.
 * @returns Display label.
 */
export function sensorsConnectionLabel(level: SensorsConnectionLevel): string {
  switch (level) {
    case "healthy":
      return "Connected";
    case "degraded":
      return "Degraded";
    default:
      return "Disconnected";
  }
}

/**
 * Derives dashboard connection level from sensors snapshot and hydration state.
 * @param snapshot - Current sensors snapshot, if any.
 * @param options - Derivation options.
 * @returns Connection level for status pill styling.
 */
export function deriveSensorsConnectionLevelFromSnapshot(
  snapshot: SensorsSnapshot | null | undefined,
  options: DeriveSensorsConnectionOptions = {}
): SensorsConnectionLevel {
  const { servicesHydrated = true } = options;

  if (!snapshot || !servicesHydrated) {
    return "offline";
  }

  const moduleStatus = snapshot.moduleHealth?.status;
  if (moduleStatus === "planned") {
    return "offline";
  }

  const sensorsStatus = snapshot.services.sensors;
  if (sensorsStatus === "healthy") {
    return snapshot.hardwareReady ? "healthy" : "degraded";
  }

  if (sensorsStatus === "degraded" || sensorsStatus === "unknown") {
    return snapshot.sensorCount > 0 ? "degraded" : "offline";
  }

  return "offline";
}
