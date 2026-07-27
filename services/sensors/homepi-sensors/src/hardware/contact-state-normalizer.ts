import type { ContactState } from "../types/contact-sensor-types.js";

/**
 * Normalizes raw GPIO/MCP bit values into open/closed contact states.
 */
export class ContactStateNormalizer {
  private readonly debounceTimers = new Map<string, NodeJS.Timeout>();

  /**
   * Converts a raw digital value to contact state with NC/NO wiring support.
   * @param rawHigh - True when the input reads high (pull-up idle).
   * @param invertLogic - True for normally-open wiring.
   * @returns Normalized contact state.
   */
  normalizeImmediate(rawHigh: boolean, invertLogic: boolean): ContactState {
    const circuitClosed = invertLogic ? rawHigh : !rawHigh;
    return circuitClosed ? "closed" : "open";
  }

  /**
   * Schedules a debounced state commit.
   * @param sensorId - Sensor id.
   * @param debounceMs - Debounce window in milliseconds.
   * @param computeState - Function that returns the latest normalized state.
   * @param onCommit - Called when debounce completes.
   */
  scheduleDebounce(
    sensorId: string,
    debounceMs: number,
    computeState: () => ContactState,
    onCommit: (state: ContactState) => void
  ): void {
    const existing = this.debounceTimers.get(sensorId);
    if (existing) {
      clearTimeout(existing);
    }

    const timer = setTimeout(() => {
      this.debounceTimers.delete(sensorId);
      onCommit(computeState());
    }, debounceMs);

    this.debounceTimers.set(sensorId, timer);
  }

  /**
   * Clears pending debounce timers.
   */
  clear(): void {
    for (const timer of this.debounceTimers.values()) {
      clearTimeout(timer);
    }
    this.debounceTimers.clear();
  }
}
