import type { CapabilityHealth, ModuleHealth, PlatformHealthEntry } from "../types/dashboard-types.js";
import type { ConnectionState, EventEnvelope, HealthReport } from "../types/dashboard-types.js";
import { isUiVisibleEvent } from "./activity-log-filter.js";

/** Visual status for service cards and dots. */
export type ServiceVisualStatus = "online" | "warning" | "offline";

/** Log level for the activity log. */
export type LogLevel = "info" | "warning" | "error" | "debug";

/**
 * Service card data for the status page.
 */
export interface ServiceCard {
  name: string;
  status: ServiceVisualStatus;
  detail?: string;
}

/**
 * Parsed log entry for the activity log panel.
 */
export interface LogEntry {
  id: string;
  timestamp: string;
  service: string;
  level: LogLevel;
  message: string;
}

/**
 * Maps a health status string to a visual dot status.
 * @param status - Health status string.
 * @returns Visual status for UI dots.
 */
export function mapHealthToVisual(status: string | undefined): ServiceVisualStatus {
  if (!status) {
    return "offline";
  }
  if (status === "healthy" || status === "online" || status === "ready" || status === "pass") {
    return "online";
  }
  if (status === "unknown") {
    return "warning";
  }
  if (
    status === "degraded" ||
    status === "warning" ||
    status === "starting" ||
    status === "stopping" ||
    status === "warn"
  ) {
    return "warning";
  }
  return "offline";
}

/**
 * Formats uptime milliseconds for display.
 * @param uptimeMs - Uptime in milliseconds.
 * @returns Human-readable uptime string.
 */
export function formatUptime(uptimeMs: number | null | undefined): string {
  if (uptimeMs == null) {
    return "—";
  }
  const totalSeconds = Math.floor(uptimeMs / 1000);
  const days = Math.floor(totalSeconds / 86_400);
  const hours = Math.floor((totalSeconds % 86_400) / 3_600);
  const minutes = Math.floor((totalSeconds % 3_600) / 60);
  if (days > 0) {
    return `${days}d ${hours}h ${minutes}m`;
  }
  if (hours > 0) {
    return `${hours}h ${minutes}m`;
  }
  return `${minutes}m`;
}

/**
 * Formats CPU temperature for display.
 * @param tempC - Temperature in Celsius.
 * @returns Formatted temperature string.
 */
export function formatCpuTemp(tempC: number | null | undefined): string {
  if (tempC == null) {
    return "—";
  }
  return `${tempC.toFixed(1)}°C`;
}

/**
 * Maps CPU temperature to a visual status.
 * @param tempC - Temperature in Celsius.
 * @returns Visual status.
 */
export function mapCpuTempStatus(tempC: number | null | undefined): ServiceVisualStatus {
  if (tempC == null) {
    return "offline";
  }
  if (tempC >= 75) {
    return "offline";
  }
  if (tempC >= 65) {
    return "warning";
  }
  return "online";
}

/**
 * Formats an ISO timestamp for display.
 * @param iso - ISO8601 timestamp.
 * @returns Localized date/time string.
 */
export function formatTimestamp(iso: string): string {
  return new Date(iso).toLocaleString();
}

/**
 * Formats log timestamp for the activity log.
 * @param iso - ISO8601 timestamp.
 * @returns Time-only string.
 */
export function formatLogTime(iso: string): string {
  return new Date(iso).toLocaleTimeString("en-US", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

/**
 * Builds transport connection cards for SSE and WebSocket.
 * @param connections - Connection states.
 * @returns Service cards for live transports.
 */
export function buildTransportCards(connections: {
  sse: ConnectionState;
  ws: ConnectionState;
}): ServiceCard[] {
  const mapConnection = (state: ConnectionState): ServiceVisualStatus => {
    if (state === "connected") {
      return "online";
    }
    if (state === "connecting") {
      return "warning";
    }
    return "offline";
  };

  return [
    { name: "SSE /events", status: mapConnection(connections.sse) },
    { name: "WebSocket /ws", status: mapConnection(connections.ws) },
  ];
}

/**
 * Builds log entries from recent events, excluding transport noise.
 * @param events - Recent event envelopes.
 * @returns Parsed log entries.
 */
function isDebugNoiseEvent(event: EventEnvelope): boolean {
  if (event.event === "log_record") {
    const level = String((event.payload as { level?: string }).level ?? "INFO").toUpperCase();
    return level === "DEBUG" || level === "TRACE";
  }
  return false;
}

export function buildLogEntries(events: EventEnvelope[]): LogEntry[] {
  return events
    .filter((event) => isUiVisibleEvent(event))
    .filter((event) => !isDebugNoiseEvent(event))
    .map((event) => ({
      id: event.id,
      timestamp: event.timestamp,
      service: event.source,
      level: mapEventToLogLevel(event),
      message: formatEventMessage(event),
    }));
}

function mapEventToLogLevel(event: EventEnvelope): LogLevel {
  if (event.event === "log_record") {
    const level = String((event.payload as { level?: string }).level ?? "INFO").toLowerCase();
    if (level === "error") {
      return "error";
    }
    if (level === "warn") {
      return "warning";
    }
    if (level === "debug") {
      return "debug";
    }
    return "info";
  }

  const haystack = `${event.event} ${event.topic}`.toLowerCase();
  if (haystack.includes("error") || haystack.includes("failed")) {
    return "error";
  }
  if (haystack.includes("warn") || haystack.includes("degraded")) {
    return "warning";
  }
  if (haystack.includes("debug")) {
    return "debug";
  }
  return "info";
}

function formatEventMessage(event: EventEnvelope): string {
  if (event.event === "log_record") {
    const payload = event.payload as { message?: string; event?: string; module?: string };
    const summary = payload.message ?? payload.event ?? "log";
    return payload.module ? `${payload.module}: ${summary}` : summary;
  }

  if (typeof event.payload.userMessage === "string") {
    return event.payload.userMessage;
  }

  if (event.topic.startsWith("homepi.audio.") || event.topic.startsWith("modules.audio.")) {
    const payload = event.payload as Record<string, unknown>;
    if (event.event === "zone_volume_changed" && typeof payload.zone === "number") {
      return `Zone ${payload.zone} volume → ${String(payload.volume ?? "?")}`;
    }
    if (event.event === "zone_power_changed" && typeof payload.zone === "number") {
      return `Zone ${payload.zone} power → ${String(payload.power ?? "?")}`;
    }
  }

  if (event.topic.startsWith("homepi.sensors.")) {
    return `${event.topic}: ${event.event}`;
  }

  if (event.topic.startsWith("homepi.health.")) {
    return String(event.payload.userMessage ?? `Health update: ${event.event}`);
  }

  return `${event.topic} · ${event.event}`;
}

/**
 * Summarizes overall dashboard health for the header.
 * @param health - Health report.
 * @param coreStatus - Core status payload.
 * @returns Summary label.
 */
export function summarizeOverallHealth(
  health: HealthReport | null,
  coreStatus: { modules?: ModuleHealth[]; healthServiceReachable?: boolean } | null
): string {
  if (coreStatus?.healthServiceReachable === false) {
    return "degraded";
  }
  if (coreStatus?.modules?.some((module) => module.status === "offline" || module.status === "failed")) {
    return "degraded";
  }
  return health?.status ?? "unknown";
}

/**
 * Returns whether any module or transport is in a warning/error state.
 * @param state - Partial dashboard state.
 * @returns True when the header should show a warning indicator.
 */
export function hasDashboardWarnings(state: {
  error: string | null;
  transportError: string | null;
  sseState: ConnectionState;
  wsState: ConnectionState;
  coreStatus: { modules?: ModuleHealth[]; healthServiceReachable?: boolean } | null;
}): boolean {
  if (state.error || state.transportError) {
    return true;
  }
  if (state.sseState === "error" || state.wsState === "error") {
    return true;
  }
  if (state.coreStatus?.healthServiceReachable === false) {
    return true;
  }
  return (
    state.coreStatus?.modules?.some(
      (module) => module.status !== "healthy" && module.status !== "unknown"
    ) ?? false
  );
}

/**
 * CSS class for a status dot color.
 * @param status - Visual status.
 * @returns Tailwind background class.
 */
export function statusDotClass(status: ServiceVisualStatus): string {
  if (status === "online") {
    return "bg-emerald-500";
  }
  if (status === "warning") {
    return "bg-amber-500";
  }
  return "bg-red-500";
}

/**
 * CSS class for a header icon color.
 * @param status - Visual status.
 * @returns Tailwind text color class.
 */
export function statusIconClass(status: ServiceVisualStatus): string {
  if (status === "online") {
    return "text-emerald-500";
  }
  if (status === "warning") {
    return "text-amber-500";
  }
  return "text-red-500";
}

/**
 * Input fields used to render health detail under a service name.
 */
export interface ServiceHealthDetailInput {
  /** Raw health status string. */
  status: string;
  /** User-facing evidence or error message. */
  userMessage?: string;
  /** Process layer status. */
  process?: string;
  /** Readiness layer status. */
  readiness?: string;
  /** Domain layer status. */
  domain?: string;
  /** ISO timestamp of the last health check. */
  lastUpdated?: string;
}

/**
 * Formats a relative "checked at" suffix for health detail lines.
 * @param iso - ISO8601 timestamp.
 * @returns Relative time label.
 */
export function formatHealthCheckedAt(iso: string): string {
  const deltaMs = Date.now() - new Date(iso).getTime();
  if (deltaMs < 60_000) {
    return "just now";
  }
  const minutes = Math.floor(deltaMs / 60_000);
  if (minutes < 60) {
    return `${minutes}m ago`;
  }
  return new Date(iso).toLocaleTimeString("en-US", {
    hour: "2-digit",
    minute: "2-digit",
  });
}

/**
 * Formats layered health evidence when no user message is available.
 * @param process - Process layer status.
 * @param readiness - Readiness layer status.
 * @param domain - Domain layer status.
 * @returns Evidence string or null.
 */
function formatHealthLayerEvidence(
  process?: string,
  readiness?: string,
  domain?: string
): string | null {
  const parts: string[] = [];
  if (process === "active") {
    parts.push("Process active");
  }
  if (readiness === "ready") {
    parts.push("Command socket ready");
  }
  if (domain === "ready") {
    parts.push("Domain checks passing");
  } else if (readiness === "ready" && domain === "unknown") {
    parts.push("Responding on command socket");
  }
  return parts.length > 0 ? parts.join(" · ") : null;
}

/**
 * Formats the subtitle shown under a service or capability name.
 * @param entry - Health detail input.
 * @returns Detail line or null when nothing should be shown.
 */
export function formatServiceHealthDetail(entry: ServiceHealthDetailInput): string | null {
  const checkedSuffix = entry.lastUpdated
    ? ` · Checked ${formatHealthCheckedAt(entry.lastUpdated)}`
    : "";

  if (entry.userMessage) {
    return `${entry.userMessage}${checkedSuffix}`;
  }

  const layerEvidence = formatHealthLayerEvidence(entry.process, entry.readiness, entry.domain);
  if (layerEvidence) {
    return `${layerEvidence}${checkedSuffix}`;
  }

  if (mapHealthToVisual(entry.status) === "online") {
    return `Operating normally${checkedSuffix}`;
  }

  return null;
}

/**
 * Summarizes header icon status from health and transport state.
 * @param healthStatus - Overall health string.
 * @param hasWarnings - Whether warnings are present.
 * @returns Visual status for the header icon.
 */
export function summarizeHeaderIconStatus(
  healthStatus: string | undefined,
  hasWarnings: boolean
): ServiceVisualStatus {
  if (hasWarnings) {
    return "warning";
  }
  return mapHealthToVisual(healthStatus);
}

export type { CapabilityHealth, ModuleHealth, PlatformHealthEntry };
