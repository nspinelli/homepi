/**
 * API response envelope from GET /api/health and related endpoints.
 */
export interface ApiResponse<T = Record<string, unknown>> {
  ok: boolean;
  correlationId: string;
  timestamp: string;
  data?: T;
  error?: {
    code: string;
    message: string;
  };
}

/**
 * Health report payload from core/health.
 */
export interface HealthReport {
  service: string;
  status: "healthy" | "degraded" | "failed" | "starting" | "stopping";
  checkedAt: string;
  checks: Array<{
    name: string;
    status: "pass" | "warn" | "fail";
    message?: string;
    durationMs?: number;
  }>;
}

/**
 * System status snapshot from core/status stream.
 */
export interface SystemStatusSnapshot {
  backend: "healthy" | "degraded" | "failed";
  config: "loaded" | "invalid";
  logging: "active" | "inactive";
  runtime: "starting" | "running" | "stopping" | "stopped" | "failed";
  transport: "ready" | "degraded" | "failed";
  events: "ready" | "degraded" | "failed";
  state: "ready" | "degraded" | "failed";
  api: "ready" | "degraded" | "failed";
  usbDevices: "healthy" | "degraded" | "offline";
  hifiSerial: "healthy" | "degraded" | "offline";
  nqptp: "healthy" | "degraded" | "offline";
  metadata: "healthy" | "degraded" | "offline";
  pcmRouter: "healthy" | "degraded" | "offline";
  uptimeMs: number;
  lastEventAt: string | null;
}

/**
 * USB device record from GET /api/usb-devices.
 */
export interface UsbDevice {
  deviceId: string;
  displayName: string;
  kind: "serial" | "audio";
  present: boolean;
}

/**
 * USB role assignments from GET /api/usb-devices/assignments.
 */
export interface UsbAssignments {
  serial: string | null;
  audioPrimary: string | null;
  paging: string | null;
}

/**
 * HomePi event envelope received over SSE.
 */
export interface EventEnvelope {
  version: number;
  id: string;
  source: string;
  topic: string;
  event: string;
  correlationId: string;
  timestamp: string;
  payload: Record<string, unknown>;
}

/**
 * Core status payload from GET /api/core/status.
 */
export interface CoreStatusPayload {
  service: string;
  checkedAt: string;
  services: Array<{
    name: string;
    status: string;
    message?: string;
  }>;
  system: SystemStatusSnapshot;
}

/**
 * Connection state for live transports.
 */
export type ConnectionState = "connecting" | "connected" | "disconnected" | "error";
