import type { ReconnectPolicy } from "./transport-types.js";

const DEFAULT_POLICY: ReconnectPolicy = {
  maxAttempts: 10,
  initialDelayMs: 500,
  maxDelayMs: 30_000,
  multiplier: 2,
};

/**
 * Computes delay before the next reconnect attempt.
 * @param attempt - Zero-based attempt number.
 * @param policy - Reconnect policy.
 * @returns Delay in milliseconds.
 */
export function computeReconnectDelay(
  attempt: number,
  policy: ReconnectPolicy = DEFAULT_POLICY
): number {
  const delay =
    policy.initialDelayMs * Math.pow(policy.multiplier, Math.max(0, attempt));
  return Math.min(delay, policy.maxDelayMs);
}

/**
 * Returns whether another reconnect attempt is allowed.
 * @param attempt - Zero-based attempt number.
 * @param policy - Reconnect policy.
 * @returns True if reconnect should be attempted.
 */
export function shouldReconnect(
  attempt: number,
  policy: ReconnectPolicy = DEFAULT_POLICY
): boolean {
  return attempt < policy.maxAttempts;
}
