import type { ServiceConfig } from "@homepi/core-config";
import { createHealthReport } from "@homepi/core-health";

import type { ModuleHealth, ServiceHealthEntry, SystemHealthSnapshot } from "./health/health-client.js";
import type { CoreStatusPayload } from "./types/system-status-types.js";

/**
 * Host metrics computed by the backend.
 */
export interface HostMetrics {
  uptimeMs: number;
  cpuTempC: number | null;
  lastEventAt: string | null;
}

/**
 * Builds a health report for GET /api/health from a system health snapshot.
 * @param config - Service configuration.
 * @param snapshot - Health snapshot from homepi-health.
 * @returns Health report payload.
 */
export function buildHealthReportFromSnapshot(
  config: ServiceConfig,
  snapshot: SystemHealthSnapshot
) {
  const moduleChecks = snapshot.modules.map((module: ModuleHealth) => ({
    name: module.module,
    status: module.planned
      ? ("warn" as const)
      : module.status === "healthy"
        ? ("pass" as const)
        : module.status === "degraded"
          ? ("warn" as const)
          : ("fail" as const),
    message:
      module.userMessage ??
      (module.planned
        ? `${module.displayName} is planned but not installed yet.`
        : `${module.displayName} is ${module.status}`),
  }));

  const checks = [
    {
      name: "http",
      status: "pass" as const,
      message: "Backend listening",
    },
    {
      name: "health-observer",
      status: snapshot.healthServiceReachable ? ("pass" as const) : ("fail" as const),
      message: snapshot.healthServiceReachable
        ? "homepi-health reachable"
        : "homepi-health unreachable",
    },
    ...moduleChecks,
  ];

  return createHealthReport({
    service: config.service,
    checks,
  });
}

/**
 * Builds hierarchical core status for GET /api/core/status.
 * @param config - Service configuration.
 * @param snapshot - Health snapshot from homepi-health.
 * @param host - Host metrics from backend.
 * @returns Core status payload.
 */
export function buildCoreStatusPayload(
  config: ServiceConfig,
  snapshot: SystemHealthSnapshot,
  host: HostMetrics
): CoreStatusPayload {
  return {
    service: config.service,
    checkedAt: snapshot.checkedAt,
    healthServiceReachable: snapshot.healthServiceReachable,
    modules: snapshot.modules,
    platform: snapshot.platform,
    services: snapshot.services.map((entry: ServiceHealthEntry) => ({
      name: entry.service,
      status: entry.status,
      message: entry.userMessage,
    })),
    host,
  };
}
