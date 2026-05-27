import type { HealthCheck, HealthReportStatus } from "./health-types.js";

/**
 * Aggregates individual check statuses into a service health status.
 * @param checks - Health checks to aggregate.
 * @returns Aggregate health status.
 */
export function aggregateHealth(checks: HealthCheck[]): HealthReportStatus {
  if (checks.some((check) => check.status === "fail")) {
    return "failed";
  }
  if (checks.some((check) => check.status === "warn")) {
    return "degraded";
  }
  return "healthy";
}
