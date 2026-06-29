import { randomUUID } from "node:crypto";

/**
 * Creates a unique correlation identifier.
 * @param prefix - Optional prefix segment.
 * @returns Correlation id string.
 */
export function createCorrelationId(prefix = "req"): string {
  return `${prefix}_${randomUUID().replace(/-/g, "").slice(0, 12)}`;
}

/**
 * Creates a unique request id.
 * @returns Request id string.
 */
export function createRequestId(): string {
  return `req_${randomUUID().replace(/-/g, "").slice(0, 12)}`;
}

/**
 * Creates a unique event id.
 * @returns Event id string.
 */
export function createEventId(): string {
  return `evt_${randomUUID().replace(/-/g, "").slice(0, 12)}`;
}
