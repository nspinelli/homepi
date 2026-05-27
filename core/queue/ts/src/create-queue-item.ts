import type { QueueItem } from "./queue-types.js";

/**
 * Creates a schema-compliant queue item.
 * @param params - Queue item fields.
 * @returns Queue item.
 */
export function createQueueItem(params: {
  id: string;
  type: string;
  payload: Record<string, unknown>;
  priority?: number;
  correlationId?: string;
  createdAt?: string;
}): QueueItem {
  return {
    id: params.id,
    type: params.type,
    payload: params.payload,
    priority: params.priority,
    correlationId: params.correlationId,
    createdAt: params.createdAt ?? new Date().toISOString(),
  };
}
