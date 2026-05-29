import type { ConnectionState, EventEnvelope, SystemStatusSnapshot } from "../types/dashboard-types.js";

/** Visual health indicator for a service card. */
export type ServiceVisualStatus = "online" | "warning" | "offline";

/** Log severity used by the status page filters. */
export type LogLevel = "info" | "warning" | "error" | "debug";

/** Core service row shown on the status page. */
export interface ServiceCardModel {
  /** Stable service key. */
  key: string;
  /** Display name. */
  name: string;
  /** Mapped visual status. */
  status: ServiceVisualStatus;
  /** Raw status string from the snapshot. */
  state: string;
  /** Secondary metric label. */
  metricLabel: string;
  /** Secondary metric value. */
  metricValue: string;
}

/** Event row shown in the live log panel. */
export interface StatusLogEntry {
  /** Unique row id. */
  id: string;
  /** ISO timestamp. */
  timestamp: string;
  /** Mapped log level. */
  level: LogLevel;
  /** Service or source label. */
  service: string;
  /** Human-readable message. */
  message: string;
}

const CORE_SERVICES: Array<{ key: keyof SystemStatusSnapshot; name: string }> = [
  { key: "backend", name: "Backend" },
  { key: "config", name: "Config" },
  { key: "logging", name: "Logging" },
  { key: "runtime", name: "Runtime" },
  { key: "transport", name: "Transport" },
  { key: "events", name: "Events" },
  { key: "state", name: "State" },
  { key: "api", name: "API" },
  { key: "usbDevices", name: "USB Devices" },
  { key: "hifiSerial", name: "HiFi Serial" },
  { key: "nqptp", name: "NQPTP" },
  { key: "metadata", name: "Metadata" },
  { key: "pcmRouter", name: "PCM Router" },
];

const EVENT_SOURCE_LABELS: Record<string, string> = {
  "homepi-hifi-serial": "HiFi Serial",
  "homepi-backend": "Backend",
  "homepi-usb-devices": "USB Devices",
  "homepi-nqptp": "NQPTP",
  "homepi-metadata": "Metadata",
  "homepi-pcm-router": "PCM Router",
};

const ONLINE_STATES = new Set([
  "healthy",
  "loaded",
  "active",
  "running",
  "ready",
  "connected",
  "present",
]);

const OFFLINE_STATES = new Set(["offline", "stopped", "failed"]);

const WARNING_STATES = new Set([
  "degraded",
  "starting",
  "stopping",
  "connecting",
  "pending",
  "disconnected",
]);

/**
 * Maps a raw platform status string to a visual indicator.
 * @param value - Raw status value.
 * @returns Visual status for cards.
 */
export function mapToVisualStatus(value: string | undefined): ServiceVisualStatus {
  if (!value) {
    return "offline";
  }
  if (ONLINE_STATES.has(value)) {
    return "online";
  }
  if (WARNING_STATES.has(value) || value === "degraded") {
    return "warning";
  }
  if (OFFLINE_STATES.has(value)) {
    return "offline";
  }
  return "offline";
}

/**
 * Builds service cards from the authoritative system status snapshot.
 * @param snapshot - Live system status snapshot.
 * @param connections - Live transport connection states.
 * @returns Service card models for the status grid.
 */
export function buildServiceCards(
  snapshot: SystemStatusSnapshot | null,
  connections: { sse: ConnectionState; ws: ConnectionState }
): ServiceCardModel[] {
  const coreCards = CORE_SERVICES.map(({ key, name }) => {
    const state = snapshot?.[key];
    const stateLabel = state === undefined ? "unknown" : String(state);

    return {
      key,
      name,
      status: mapToVisualStatus(stateLabel),
      state: stateLabel,
      metricLabel: "State",
      metricValue: stateLabel,
    };
  });

  return [
    ...coreCards,
    {
      key: "sse",
      name: "SSE /events",
      status: mapToVisualStatus(connections.sse),
      state: connections.sse,
      metricLabel: "Transport",
      metricValue: "Server-sent events",
    },
    {
      key: "ws",
      name: "WebSocket /ws",
      status: mapToVisualStatus(connections.ws),
      state: connections.ws,
      metricLabel: "Transport",
      metricValue: "WebSocket",
    },
  ];
}

/**
 * Converts SSE envelopes into log rows for the status page.
 * @param events - Recent event envelopes.
 * @returns Log entries newest-first.
 */
export function buildLogEntries(events: EventEnvelope[]): StatusLogEntry[] {
  return events.map((event) => ({
    id: event.id,
    timestamp: event.timestamp,
    level: mapEventToLogLevel(event),
    service: EVENT_SOURCE_LABELS[event.source] ?? event.source ?? event.topic,
    message: formatEventMessage(event),
  }));
}

/**
 * Formats uptime milliseconds for display.
 * @param uptimeMs - Uptime in milliseconds.
 * @returns Human-readable uptime string.
 */
export function formatUptime(uptimeMs: number | undefined): string {
  if (uptimeMs === undefined) {
    return "—";
  }
  const totalSeconds = Math.floor(uptimeMs / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  return `${hours}h ${minutes}m ${seconds}s`;
}

/**
 * Formats an ISO timestamp for display.
 * @param value - ISO8601 timestamp.
 * @returns Locale formatted timestamp.
 */
export function formatTimestamp(value: string): string {
  return new Date(value).toLocaleString();
}

/**
 * Formats a time-only value for log rows.
 * @param iso - ISO timestamp.
 * @returns Locale time string.
 */
export function formatLogTime(iso: string): string {
  return new Date(iso).toLocaleTimeString("en-US", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

function mapEventToLogLevel(event: EventEnvelope): LogLevel {
  if (event.event === "log_record") {
    const level = String(
      (event.payload as { level?: string }).level ?? "INFO"
    ).toLowerCase();
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
  if (haystack.includes("debug") || haystack.includes("heartbeat")) {
    return "debug";
  }
  return "info";
}

function formatEventMessage(event: EventEnvelope): string {
  if (event.event === "log_record") {
    const payload = event.payload as {
      message?: string;
      event?: string;
      module?: string;
    };
    const summary = payload.message ?? payload.event ?? "log";
    return payload.module ? `${payload.module}: ${summary}` : summary;
  }
  if (event.event === "system_status_snapshot" || event.event === "system_status_delta") {
    return `${event.event} received`;
  }
  if (event.event === "heartbeat") {
    return "Heartbeat";
  }
  if (event.topic.startsWith("modules.audio.")) {
    const payload = event.payload as Record<string, unknown>;
    if (event.event === "zone_volume_changed" && typeof payload.zone === "number") {
      return `Zone ${payload.zone} volume → ${String(payload.volume ?? "?")}`;
    }
    if (event.event === "zone_power_changed" && typeof payload.zone === "number") {
      return `Zone ${payload.zone} power → ${String(payload.power ?? "?")}`;
    }
    if (event.event === "zone_name_changed" && typeof payload.zone === "number") {
      return `Zone ${payload.zone} renamed → ${String(payload.name ?? "")}`;
    }
    if (event.event === "zone_source_changed" && typeof payload.zone === "number") {
      return `Zone ${payload.zone} source → ${String(payload.source ?? "?")}`;
    }
    if (event.event === "source_name_changed" && typeof payload.source === "number") {
      return `Source ${payload.source} renamed → ${String(payload.name ?? "")}`;
    }
    if (event.event === "audio_state_snapshot") {
      return "Audio state snapshot";
    }
    if (event.event === "language_strings_synced") {
      return "Language strings synced";
    }
  }
  if (event.topic.startsWith("modules.pcm")) {
    const payload = event.payload as Record<string, unknown>;
    if (event.event === "owner_changed") {
      return `PCM owner → zone ${String(payload.ownerZoneId ?? "?")}`;
    }
    if (event.event === "owner_cleared") {
      return "PCM owner cleared";
    }
    if (event.event === "dac_state") {
      return `DAC ${String(payload.state ?? "?")}`;
    }
    if (event.event === "pcm_router_snapshot") {
      return "PCM router snapshot";
    }
  }
  return `${event.event} on ${event.topic}`;
}
