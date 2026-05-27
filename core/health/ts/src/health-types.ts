/**
 * Health check status per health-check.schema.json.
 */
export type HealthCheckStatus = "pass" | "warn" | "fail";

/**
 * Health check entry per health-check.schema.json.
 */
export interface HealthCheck {
  /** Check name. */
  name: string;
  /** Check status. */
  status: HealthCheckStatus;
  /** Optional status message. */
  message?: string;
  /** Check duration in milliseconds. */
  durationMs?: number;
  /** Optional structured check data. */
  data?: Record<string, unknown>;
}

/**
 * Aggregate health status per health-report.schema.json.
 */
export type HealthReportStatus =
  | "healthy"
  | "degraded"
  | "failed"
  | "starting"
  | "stopping";

/**
 * Health report per health-report.schema.json.
 */
export interface HealthReport {
  /** Reporting service name. */
  service: string;
  /** Aggregate health status. */
  status: HealthReportStatus;
  /** ISO8601 UTC timestamp. */
  checkedAt: string;
  /** Individual health checks. */
  checks: HealthCheck[];
}
