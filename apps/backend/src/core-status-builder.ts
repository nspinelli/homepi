import type { ServiceConfig } from "@homepi/core-config";
import type {
  CoreServiceStatusEntry,
  CoreStatusPayload,
  SystemStatusSnapshot,
} from "./types/system-status-types.js";

/**
 * Builds aggregated core platform status from the current system snapshot.
 * @param config - Loaded service configuration.
 * @param system - Current system status snapshot.
 * @returns Core status payload for GET /api/core/status.
 */
export function buildCoreStatusPayload(
  config: ServiceConfig,
  system: SystemStatusSnapshot
): CoreStatusPayload {
  const services: CoreServiceStatusEntry[] = [
    { name: "config", status: system.config, message: "Service configuration" },
    { name: "logging", status: system.logging, message: "Structured logging" },
    { name: "runtime", status: system.runtime, message: "Process lifecycle" },
    { name: "transport", status: system.transport, message: "SSE and WebSocket" },
    { name: "events", status: system.events, message: "Event envelopes" },
    { name: "state", status: system.state, message: "In-memory status store" },
    { name: "api", status: system.api, message: "HTTP API envelopes" },
  ];

  return {
    service: config.service,
    checkedAt: new Date().toISOString(),
    services,
    system,
  };
}
