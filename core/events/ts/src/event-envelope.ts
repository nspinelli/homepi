import { randomUUID } from "node:crypto";
import type { EventEnvelope } from "./event-types.js";

/**
 * Creates a schema-compliant event envelope.
 * @param params - Required envelope fields.
 * @returns Event envelope.
 */
export function createEventEnvelope(params: {
  source: string;
  topic: string;
  event: string;
  correlationId: string;
  payload: Record<string, unknown>;
  id?: string;
  version?: number;
  timestamp?: string;
}): EventEnvelope {
  return {
    version: params.version ?? 1,
    id: params.id ?? randomUUID(),
    source: params.source,
    topic: params.topic,
    event: params.event,
    correlationId: params.correlationId,
    timestamp: params.timestamp ?? new Date().toISOString(),
    payload: params.payload,
  };
}
