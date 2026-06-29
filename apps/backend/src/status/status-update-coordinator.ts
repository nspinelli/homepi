import type { EventBroadcaster } from "../event-broadcaster.js";
import type { SystemStatusStore } from "../system-status-store.js";
import type { HostMetricsSnapshot } from "../types/system-status-types.js";
import type { WsHandler } from "../ws-handler.js";

const HOST_METRICS_FIELDS: (keyof HostMetricsSnapshot)[] = [
  "cpuTempC",
  "lastEventAt",
];

/**
 * Options for the status update coordinator.
 */
export interface StatusUpdateCoordinatorOptions {
  /** Authoritative host metrics store. */
  statusStore: SystemStatusStore;
  /** SSE event broadcaster. */
  broadcaster: EventBroadcaster;
  /** Optional WebSocket handler for live deltas. */
  wsHandler?: WsHandler;
}

/**
 * Single write path for host metric patches with change detection and live broadcast.
 */
export class StatusUpdateCoordinator {
  /**
   * @param options - Coordinator dependencies.
   */
  constructor(private readonly options: StatusUpdateCoordinatorOptions) {}

  /**
   * Patches host metrics and broadcasts a delta when tracked fields change.
   * @param patch - Partial host metrics update.
   * @param source - Update source label for correlation ids.
   * @param eventTimestamp - Optional ISO timestamp for lastEventAt.
   */
  patchAndBroadcast(
    patch: Partial<HostMetricsSnapshot>,
    source: string,
    eventTimestamp?: string
  ): void {
    const before = this.options.statusStore.getStatus();
    const merged: Partial<HostMetricsSnapshot> = {
      ...patch,
      ...(eventTimestamp ? { lastEventAt: eventTimestamp } : {}),
    };

    if (!this.hasHostFieldChange(before, merged)) {
      if (eventTimestamp && before.lastEventAt !== eventTimestamp) {
        this.options.statusStore.patchStatus({ lastEventAt: eventTimestamp });
        this.broadcastDelta(source, eventTimestamp);
      }
      return;
    }

    this.options.statusStore.patchStatus(merged);
    this.broadcastDelta(source, eventTimestamp);
  }

  private broadcastDelta(source: string, eventTimestamp?: string): void {
    const correlationId = `${source}-${Date.now()}`;
    const emittedAt = eventTimestamp ?? new Date().toISOString();
    this.options.broadcaster.broadcastStatusDelta(correlationId, emittedAt);
    this.options.wsHandler?.broadcastStatusDelta(correlationId);
  }

  private hasHostFieldChange(
    before: HostMetricsSnapshot,
    patch: Partial<HostMetricsSnapshot>
  ): boolean {
    for (const key of HOST_METRICS_FIELDS) {
      if (key in patch && patch[key] !== before[key]) {
        return true;
      }
    }
    return false;
  }
}
