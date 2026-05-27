import { randomUUID } from "node:crypto";
import type { TransportEnvelope, TransportMessageType } from "./transport-types.js";

/**
 * Creates a transport envelope with required fields.
 * @param params - Envelope fields.
 * @returns Transport envelope.
 */
export function createTransportEnvelope(params: {
  type: TransportMessageType;
  source: string;
  topic: string;
  correlationId: string;
  payload: Record<string, unknown>;
  target?: string;
  id?: string;
  version?: number;
}): TransportEnvelope {
  return {
    version: params.version ?? 1,
    id: params.id ?? randomUUID(),
    type: params.type,
    source: params.source,
    target: params.target,
    topic: params.topic,
    correlationId: params.correlationId,
    timestamp: new Date().toISOString(),
    payload: params.payload,
  };
}
