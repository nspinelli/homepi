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
  healthServiceReachable?: boolean;
}

/**
 * Capability health from hierarchical status API.
 */
export interface CapabilityHealth {
  id: string;
  displayName: string;
  status: string;
  userMessage?: string;
  process?: string;
  readiness?: string;
  domain?: string;
  lastUpdated: string;
}

/**
 * Module health rollup from hierarchical status API.
 */
export interface ModuleHealth {
  module: string;
  displayName: string;
  icon: string;
  status: string;
  /** True when the module facade is not yet installed. */
  planned?: boolean;
  userMessage?: string;
  stillWorks?: string[];
  availableActions?: string[];
  capabilities: CapabilityHealth[];
  lastUpdated: string;
}

/**
 * Platform infrastructure health entry.
 */
export interface PlatformHealthEntry {
  name: string;
  status: string;
  userMessage?: string;
  lastUpdated: string;
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
 * Supported PCM profile tuple.
 */
export interface AudioProfileTuple {
  sampleRate: number;
  channels: number;
  sampleFormat: "S16_LE" | "S32_LE";
}

/**
 * USB role assignments from GET /api/usb-devices/assignments.
 */
export interface UsbAssignments {
  serial: string | null;
  audioPrimary: string | null;
  paging: string | null;
  audioPrimaryProfile?: AudioProfileTuple | null;
}

/**
 * Audio capabilities for a USB device.
 */
export interface AudioCapabilities {
  deviceId: string;
  supportedProfileTuples: AudioProfileTuple[];
  probedAt?: string;
  probeError?: string;
}

/**
 * Host metrics streamed from the backend status store.
 */
export interface HostMetricsSnapshot {
  uptimeMs: number;
  cpuTempC: number | null;
  lastEventAt: string | null;
}

/** @deprecated Use HostMetricsSnapshot. */
export type SystemStatusSnapshot = HostMetricsSnapshot;

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
  healthServiceReachable: boolean;
  modules: ModuleHealth[];
  platform: PlatformHealthEntry[];
  services: Array<{
    name: string;
    status: string;
    message?: string;
  }>;
  host: HostMetricsSnapshot;
}

/**
 * Connection state for live transports.
 */
export type ConnectionState = "connecting" | "connected" | "disconnected" | "error";

/**
 * Dashboard fetch/load state for UI messaging.
 */
export type DashboardLoadState = "loading" | "ready" | "error" | "stale";
