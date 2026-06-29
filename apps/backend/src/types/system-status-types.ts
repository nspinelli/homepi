/**
 * Host metrics computed and cached by the backend.
 */
export interface HostMetricsSnapshot {
  /** Process uptime in milliseconds. */
  uptimeMs: number;
  /** CPU temperature in degrees Celsius, when available. */
  cpuTempC: number | null;
  /** ISO8601 timestamp of the last emitted system event. */
  lastEventAt: string | null;
}

/** @deprecated Use HostMetricsSnapshot — retained for SSE/WS transport payloads. */
export type SystemStatusSnapshot = HostMetricsSnapshot;

/**
 * Core service status entry returned by GET /api/core/status.
 */
export interface CoreServiceStatusEntry {
  /** Core module identifier. */
  name: string;
  /** Service readiness. */
  status: string;
  /** Optional detail message. */
  message?: string;
}

/**
 * Aggregated core platform status payload.
 */
export interface CoreStatusPayload {
  /** Reporting service. */
  service: string;
  /** ISO8601 timestamp. */
  checkedAt: string;
  /** Whether homepi-health was reachable for this payload. */
  healthServiceReachable: boolean;
  /** Client-facing module health rollups. */
  modules: Array<{
    module: string;
    displayName: string;
    icon: string;
    status: string;
    userMessage?: string;
    stillWorks?: string[];
    availableActions?: string[];
    capabilities: Array<{
      id: string;
      displayName: string;
      status: string;
      userMessage?: string;
      process?: string;
      readiness?: string;
      domain?: string;
      lastUpdated: string;
    }>;
    lastUpdated: string;
  }>;
  /** Platform infrastructure entries. */
  platform: Array<{
    name: string;
    status: string;
    userMessage?: string;
    lastUpdated: string;
  }>;
  /** Individual service entries. */
  services: CoreServiceStatusEntry[];
  /** Host metrics computed by the backend. */
  host: HostMetricsSnapshot;
}
