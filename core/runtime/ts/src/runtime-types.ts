/**
 * Service lifecycle states per runtime contracts.
 */
export type LifecycleState =
  | "starting"
  | "running"
  | "stopping"
  | "stopped"
  | "failed";

/**
 * Runtime status payload shape (data contract only).
 */
export interface RuntimeStatus {
  service: string;
  state: LifecycleState;
  pid?: number;
  startedAt?: string;
  message?: string;
}

/**
 * Watchdog status payload shape.
 */
export interface WatchdogStatus {
  service: string;
  enabled: boolean;
  lastPingAt?: string;
  timeoutMs?: number;
  healthy: boolean;
}

/**
 * Health state summary for runtime integration.
 */
export interface HealthState {
  service: string;
  status: "healthy" | "degraded" | "failed" | "unknown";
  checkedAt: string;
}
