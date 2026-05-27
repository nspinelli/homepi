import type { LifecycleState } from "./runtime-types.js";

/** Valid lifecycle state transitions. */
const TRANSITIONS: Record<LifecycleState, LifecycleState[]> = {
  starting: ["running", "failed", "stopped"],
  running: ["stopping", "failed"],
  stopping: ["stopped", "failed"],
  stopped: ["starting"],
  failed: ["starting", "stopped"],
};

/**
 * Returns whether a lifecycle transition is valid.
 * @param from - Current state.
 * @param to - Target state.
 * @returns True when transition is allowed.
 */
export function isValidLifecycleTransition(
  from: LifecycleState,
  to: LifecycleState
): boolean {
  return TRANSITIONS[from].includes(to);
}

/**
 * Creates a runtime status object for a service.
 * @param service - Service name.
 * @param state - Lifecycle state.
 * @returns Runtime status payload.
 */
export function createRuntimeStatus(
  service: string,
  state: LifecycleState
): { service: string; state: LifecycleState; startedAt?: string } {
  return {
    service,
    state,
    startedAt: state === "running" ? new Date().toISOString() : undefined,
  };
}
