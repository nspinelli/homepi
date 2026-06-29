import { createSnapshot } from "@homepi/core-state";
import type { StateSnapshot } from "@homepi/core-state";
import type { HostMetricsSnapshot } from "./types/system-status-types.js";

const STATE_OWNER = "homepi-backend";
const STATE_TOPIC = "system.status";

/**
 * In-memory authoritative system status backed by core/state snapshots.
 */
export class SystemStatusStore {
  private snapshot: StateSnapshot;
  private readonly startedAt: Date | null;

  /**
   * Creates a new system status store with an initial snapshot.
   * @param initial - Initial system status values.
   * @param startedAt - Optional process start time for computed uptime.
   */
  constructor(initial: HostMetricsSnapshot, startedAt?: Date) {
    this.startedAt = startedAt ?? null;
    this.snapshot = createSnapshot({
      owner: STATE_OWNER,
      topic: STATE_TOPIC,
      state: { ...initial },
    });
  }

  /**
   * Returns the current system status snapshot values.
   * Uptime is computed at read time when startedAt was provided.
   * @returns System status snapshot.
   */
  getStatus(): HostMetricsSnapshot {
    const status = structuredClone(this.snapshot.state) as unknown as HostMetricsSnapshot;
    if (this.startedAt) {
      status.uptimeMs = Math.max(0, Date.now() - this.startedAt.getTime());
    }
    return status;
  }

  /**
   * Returns the underlying core/state snapshot envelope.
   * @returns State snapshot envelope.
   */
  getStateSnapshot(): StateSnapshot {
    return structuredClone(this.snapshot);
  }

  /**
   * Replaces the full system status snapshot.
   * @param next - Updated status values.
   */
  setStatus(next: HostMetricsSnapshot): void {
    this.snapshot = createSnapshot({
      owner: STATE_OWNER,
      topic: STATE_TOPIC,
      state: { ...next },
    });
  }

  /**
   * Applies a partial update to the system status snapshot.
   * @param patch - Partial status fields to merge.
   */
  patchStatus(patch: Partial<HostMetricsSnapshot>): void {
    const current = this.getStatus();
    this.setStatus({ ...current, ...patch });
  }
}
