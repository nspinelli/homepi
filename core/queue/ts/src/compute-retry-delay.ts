import type { RetryPolicy } from "./queue-types.js";

/**
 * Computes the retry delay for an attempt using the documented retry policy fields.
 * @param policy - Retry policy.
 * @param attempt - One-based attempt number.
 * @returns Delay in milliseconds.
 */
export function computeRetryDelay(policy: RetryPolicy, attempt: number): number {
  const safeAttempt = Math.max(1, attempt);
  const strategy = policy.strategy ?? "fixed";

  if (strategy === "exponential") {
    return policy.backoffMs * 2 ** (safeAttempt - 1);
  }

  return policy.backoffMs;
}
