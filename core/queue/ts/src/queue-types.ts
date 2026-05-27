/**
 * Retry strategy per retry-policy.schema.json.
 */
export type RetryStrategy = "fixed" | "exponential";

/**
 * Retry policy per retry-policy.schema.json.
 */
export interface RetryPolicy {
  /** Maximum retry attempts. */
  maxAttempts: number;
  /** Base backoff in milliseconds. */
  backoffMs: number;
  /** Retry delay strategy. */
  strategy?: RetryStrategy;
}

/**
 * Queue item per queue-item.schema.json.
 */
export interface QueueItem {
  /** Queue item identifier. */
  id: string;
  /** Item type name. */
  type: string;
  /** Item priority from 0 to 100. */
  priority?: number;
  /** ISO8601 UTC creation timestamp. */
  createdAt: string;
  /** Optional correlation identifier. */
  correlationId?: string;
  /** Item payload. */
  payload: Record<string, unknown>;
}
