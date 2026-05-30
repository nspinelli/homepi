/**
 * Aggregate backend health for the system dashboard.
 */
export type BackendHealthStatus = "healthy" | "degraded" | "failed";

/**
 * Config load status for the system dashboard.
 */
export type ConfigStatus = "loaded" | "invalid";

/**
 * Generic ready status for core platform services.
 */
export type CoreReadyStatus = "ready" | "degraded" | "failed";

/**
 * USB devices service status for the system dashboard.
 */
export type UsbDevicesStatus = "healthy" | "degraded" | "offline";

/**
 * HiFi serial service status for the system dashboard.
 */
export type HifiSerialStatus = "healthy" | "degraded" | "offline";

/**
 * NQPTP (external) service status for the system dashboard.
 */
export type NqptpStatus = "healthy" | "degraded" | "offline";

/**
 * Metadata reader (external) service status for the system dashboard.
 */
export type MetadataStatus = "healthy" | "degraded" | "offline";

/**
 * PCM router service status for the system dashboard.
 */
export type PcmRouterStatus = "healthy" | "degraded" | "offline";

/**
 * Shairport supervisor service status for the system dashboard.
 */
export type ShairportStatus = "healthy" | "degraded" | "offline";

/**
 * Runtime lifecycle status exposed on the dashboard.
 */
export type RuntimeDashboardStatus = "starting" | "running" | "stopping" | "stopped" | "failed";

/**
 * Authoritative in-memory system status snapshot for the vertical slice.
 */
export interface SystemStatusSnapshot {
  /** Backend aggregate health. */
  backend: BackendHealthStatus;
  /** Config load result. */
  config: ConfigStatus;
  /** Logging subsystem status. */
  logging: "active" | "inactive";
  /** Runtime lifecycle summary. */
  runtime: RuntimeDashboardStatus;
  /** Transport layer readiness. */
  transport: CoreReadyStatus;
  /** Events subsystem readiness. */
  events: CoreReadyStatus;
  /** State store readiness. */
  state: CoreReadyStatus;
  /** API layer readiness. */
  api: CoreReadyStatus;
  /** USB devices daemon status. */
  usbDevices: UsbDevicesStatus;
  /** HiFi serial daemon status. */
  hifiSerial: HifiSerialStatus;
  /** NQPTP timing daemon status (systemd). */
  nqptp: NqptpStatus;
  /** Shairport metadata reader status (systemd). */
  metadata: MetadataStatus;
  /** PCM router daemon status (systemd). */
  pcmRouter: PcmRouterStatus;
  /** Shairport supervisor status (systemd). */
  shairport: ShairportStatus;
  /** Process uptime in milliseconds. */
  uptimeMs: number;
  /** CPU temperature in degrees Celsius, when available. */
  cpuTempC: number | null;
  /** ISO8601 timestamp of the last emitted system event. */
  lastEventAt: string | null;
}

/**
 * Core service status entry returned by GET /api/core/status.
 */
export interface CoreServiceStatusEntry {
  /** Core module identifier. */
  name: string;
  /** Service readiness. */
  status:
    | CoreReadyStatus
    | ConfigStatus
    | UsbDevicesStatus
    | HifiSerialStatus
    | NqptpStatus
    | MetadataStatus
    | PcmRouterStatus
    | ShairportStatus
    | "active"
    | "inactive"
    | RuntimeDashboardStatus;
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
  /** Individual core service entries. */
  services: CoreServiceStatusEntry[];
  /** Current system status snapshot. */
  system: SystemStatusSnapshot;
}
