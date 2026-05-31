import type { EventBroadcaster } from "../event-broadcaster.js";
import type { SystemStatusStore } from "../system-status-store.js";
import type { SystemStatusSnapshot } from "../types/system-status-types.js";
import type { WsHandler } from "../ws-handler.js";

const SERVICE_STATUS_FIELDS: (keyof SystemStatusSnapshot)[] = [
  "usbDevices",
  "hifiSerial",
  "nqptp",
  "metadata",
  "pcmRouter",
  "shairport",
  "cpuTempC",
  "lastEventAt",
];

/**
 * Options for the status update coordinator.
 */
export interface StatusUpdateCoordinatorOptions {
  /** Authoritative status store. */
  statusStore: SystemStatusStore;
  /** SSE event broadcaster. */
  broadcaster: EventBroadcaster;
  /** Optional WebSocket handler for live deltas. */
  wsHandler?: WsHandler;
}

/**
 * Single write path for system status patches with change detection and SSE broadcast.
 */
export class StatusUpdateCoordinator {
  /**
   * @param options - Coordinator dependencies.
   */
  constructor(private readonly options: StatusUpdateCoordinatorOptions) {}

  /**
   * Patches the status store and broadcasts a delta when service fields change.
   * @param patch - Partial status update.
   * @param source - Update source label for correlation ids.
   * @param eventTimestamp - Optional ISO timestamp for lastEventAt.
   */
  patchAndBroadcast(
    patch: Partial<SystemStatusSnapshot>,
    source: string,
    eventTimestamp?: string
  ): void {
    const before = this.options.statusStore.getStatus();
    const merged: Partial<SystemStatusSnapshot> = {
      ...patch,
      ...(eventTimestamp ? { lastEventAt: eventTimestamp } : {}),
    };

    if (!this.hasServiceFieldChange(before, merged)) {
      if (eventTimestamp && before.lastEventAt !== eventTimestamp) {
        this.options.statusStore.patchStatus({ lastEventAt: eventTimestamp });
      }
      return;
    }

    this.options.statusStore.patchStatus(merged);
    const correlationId = `${source}-${Date.now()}`;
    const emittedAt = eventTimestamp ?? new Date().toISOString();
    this.options.broadcaster.broadcastStatusDelta(correlationId, emittedAt);
    this.options.wsHandler?.broadcastStatusDelta(correlationId);
  }

  /**
   * Marks a service offline when its event bridge disconnects.
   * @param field - Status store service field.
   * @param source - Bridge source label.
   */
  markServiceOffline(
    field: "usbDevices" | "hifiSerial" | "pcmRouter",
    source: string
  ): void {
    this.patchAndBroadcast({ [field]: "offline" }, source);
  }

  private hasServiceFieldChange(
    before: SystemStatusSnapshot,
    patch: Partial<SystemStatusSnapshot>
  ): boolean {
    for (const key of SERVICE_STATUS_FIELDS) {
      if (key in patch && patch[key] !== before[key]) {
        return true;
      }
    }
    return false;
  }
}
