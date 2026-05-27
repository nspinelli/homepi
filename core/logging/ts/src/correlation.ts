import { randomUUID } from "node:crypto";
import { AsyncLocalStorage } from "node:async_hooks";

const correlationStorage = new AsyncLocalStorage<string>();

/**
 * Generates a new correlation ID for a logical operation.
 * @returns Unique correlation identifier.
 */
export function createCorrelationId(prefix?: string): string {
  const id = randomUUID();
  return prefix ? `${prefix}-${id}` : id;
}

/**
 * Runs a function with a correlation ID bound to async context.
 * @param correlationId - Correlation ID for the operation.
 * @param fn - Function to execute within the context.
 * @returns Result of the function.
 */
export function withCorrelationId<T>(
  correlationId: string,
  fn: () => T
): T {
  return correlationStorage.run(correlationId, fn);
}

/**
 * Returns the correlation ID from async context, if set.
 * @returns Active correlation ID or undefined.
 */
export function getCorrelationId(): string | undefined {
  return correlationStorage.getStore();
}

/**
 * Resolves correlation ID from explicit value or async context.
 * @param explicit - Explicit correlation ID override.
 * @returns Resolved correlation ID.
 */
export function resolveCorrelationId(explicit?: string): string {
  return explicit ?? getCorrelationId() ?? createCorrelationId();
}
