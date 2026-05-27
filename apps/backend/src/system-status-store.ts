import { createSnapshot } from "@homepi/core-state";
import type { StateSnapshot } from "@homepi/core-state";
import type { SystemStatusSnapshot } from "./types/system-status-types.js";

const STATE_OWNER = "homepi-backend";
const STATE_TOPIC = "system.status";

/**
 * In-memory authoritative system status backed by core/state snapshots.
 */
export class SystemStatusStore {
  private snapshot: StateSnapshot;

  /**
   * Creates a new system status store with an initial snapshot.
   * @param initial - Initial system status values.
   */
  constructor(initial: SystemStatusSnapshot) {
    this.snapshot = createSnapshot({
      owner: STATE_OWNER,
      topic: STATE_TOPIC,
      state: { ...initial },
    });
  }

  /**
   * Returns the current system status snapshot values.
   * @returns System status snapshot.
   */
  getStatus(): SystemStatusSnapshot {
    return structuredClone(this.snapshot.state) as unknown as SystemStatusSnapshot;
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
  setStatus(next: SystemStatusSnapshot): void {
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
  patchStatus(patch: Partial<SystemStatusSnapshot>): void {
    const current = this.getStatus();
    this.setStatus({ ...current, ...patch });
  }
}
