import { aggregateHealth } from "./aggregate-health.js";
import type { HealthCheck, HealthReport, HealthReportStatus } from "./health-types.js";

/**
 * Creates a health report from service checks.
 * @param params - Health report fields.
 * @returns Health report.
 */
export function createHealthReport(params: {
  service: string;
  checks: HealthCheck[];
  status?: HealthReportStatus;
  checkedAt?: string;
}): HealthReport {
  return {
    service: params.service,
    status: params.status ?? aggregateHealth(params.checks),
    checkedAt: params.checkedAt ?? new Date().toISOString(),
    checks: params.checks,
  };
}
