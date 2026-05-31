import type { HifiSerialHealth } from "../hifi-serial/hifi-serial-types.js";
import type {
  HifiSerialStatus,
  MetadataStatus,
  NqptpStatus,
  PcmRouterStatus,
  ShairportStatus,
  UsbDevicesStatus,
} from "../types/system-status-types.js";

/**
 * Maps native USB health to dashboard status.
 * @param reachable - Socket reachable flag.
 * @param degraded - Assignments degraded flag.
 * @returns Dashboard usbDevices status.
 */
export function mapUsbDevicesStatus(
  reachable: boolean,
  degraded: boolean
): UsbDevicesStatus {
  if (!reachable) {
    return "offline";
  }
  return degraded ? "degraded" : "healthy";
}

/**
 * Maps native HiFi health to dashboard status.
 * @param health - Native health snapshot.
 * @returns Dashboard hifiSerial status.
 */
export function mapHifiSerialStatus(health: HifiSerialHealth): HifiSerialStatus {
  if (!health.connected) {
    return "offline";
  }
  if (health.degraded || health.syncInProgress) {
    return "degraded";
  }
  return "healthy";
}

/**
 * Maps HiFi service health payload fields to dashboard status.
 * @param payload - Service health event payload.
 * @returns Dashboard hifiSerial status.
 */
export function mapHifiSerialFromPayload(payload: Record<string, unknown>): HifiSerialStatus {
  const connected = payload.connected === true;
  const degraded = payload.degraded === true;
  const syncInProgress = payload.syncInProgress === true;
  if (typeof payload.status === "string") {
    if (payload.status === "offline") {
      return "offline";
    }
    if (payload.status === "degraded") {
      return "degraded";
    }
    if (payload.status === "healthy") {
      return "healthy";
    }
  }
  return mapHifiSerialStatus({
    lifecycle: "running",
    connected,
    serialPath: null,
    serialAssigned: payload.serialAssigned === true,
    syncInProgress,
    degraded,
    lastFullSyncAt: null,
    queueDepth: 0,
  });
}

/**
 * Maps USB service health payload to dashboard status.
 * @param payload - Service health event payload.
 * @returns Dashboard usbDevices status.
 */
export function mapUsbDevicesFromPayload(payload: Record<string, unknown>): UsbDevicesStatus {
  if (typeof payload.status === "string") {
    if (payload.status === "offline" || payload.status === "degraded" || payload.status === "healthy") {
      return payload.status;
    }
  }
  const reachable = payload.reachable !== false;
  const degraded = payload.assignmentsDegraded === true;
  return mapUsbDevicesStatus(reachable, degraded);
}

/**
 * Maps native PCM DAC state to dashboard status.
 * @param dacState - DAC state label from pcm_router_snapshot or dac_state events.
 * @returns Dashboard pcmRouter status.
 */
export function mapPcmRouterFromDacState(dacState: string): PcmRouterStatus {
  if (dacState === "DAC_OPEN" || dacState === "DAC_IDLE") {
    return "healthy";
  }
  if (
    dacState === "DAC_UNASSIGNED" ||
    dacState === "DAC_UNAVAILABLE" ||
    dacState === "DAC_DEGRADED"
  ) {
    return "degraded";
  }
  return "offline";
}

/**
 * Maps PCM service health payload to dashboard status.
 * @param payload - Service health or dac_state event payload.
 * @param event - Event name.
 * @returns Dashboard pcmRouter status or null when not applicable.
 */
export function mapPcmRouterFromPayload(
  payload: Record<string, unknown>,
  event: string
): PcmRouterStatus | null {
  if (typeof payload.dacState === "string") {
    return mapPcmRouterFromDacState(payload.dacState);
  }

  if (typeof payload.status === "string") {
    if (payload.status === "offline" || payload.status === "degraded" || payload.status === "healthy") {
      return payload.status;
    }
    if (payload.status === "running") {
      return payload.audioActive === false ? "degraded" : "healthy";
    }
  }

  if (event === "health") {
    if (payload.status === "running" && payload.audioActive !== false) {
      return "healthy";
    }
    return "degraded";
  }

  if (event === "dac_state" || event === "dac_opened" || event === "dac_closed") {
    const state = typeof payload.state === "string" ? payload.state : "";
    if (state) {
      return mapPcmRouterFromDacState(state === "DAC_OPENED" ? "DAC_OPEN" : state);
    }
  }

  if (event === "pcm_router_snapshot") {
    return null;
  }

  if (event === "service_degraded" || event === "dac_unassigned") {
    return "degraded";
  }

  if (event === "service_recovered" || event === "service_ready") {
    return "healthy";
  }

  return null;
}

/**
 * Maps systemd active state to dashboard external service status.
 * @param state - systemctl is-active output.
 * @returns Dashboard healthy / degraded / offline.
 */
export function mapSystemdServiceStatus(
  state: string
): NqptpStatus | MetadataStatus | PcmRouterStatus | ShairportStatus {
  if (state === "active") {
    return "healthy";
  }
  if (state === "activating" || state === "reloading") {
    return "degraded";
  }
  return "offline";
}

/**
 * Maps journal lifecycle event to systemd-equivalent status.
 * @param event - Lifecycle event name from journald.
 * @returns Dashboard status or null when not mappable.
 */
export function mapLifecycleEventToStatus(
  event: string
): NqptpStatus | MetadataStatus | ShairportStatus | null {
  if (
    event === "service_started" ||
    event === "service_ready" ||
    event === "lifecycle_starting"
  ) {
    return "healthy";
  }
  if (event === "service_restarting" || event === "service_degraded") {
    return "degraded";
  }
  if (
    event === "service_stopped" ||
    event === "service_stopping" ||
    event === "service_failed" ||
    event === "lifecycle_stopping"
  ) {
    return "offline";
  }
  return null;
}

/**
 * Maps journal log service name to a system status store field.
 * @param service - Journal log service field.
 * @returns Store field name or null.
 */
export function journalServiceToStatusField(
  service: string
): "nqptp" | "metadata" | "shairport" | null {
  if (service === "homepi-nqptp") {
    return "nqptp";
  }
  if (service.startsWith("homepi-metadata")) {
    return "metadata";
  }
  if (service === "homepi-shairport-supervisor" || service.startsWith("homepi-shairport")) {
    return "shairport";
  }
  return null;
}
