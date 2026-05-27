/**
 * Transport envelope message types.
 */
export type TransportMessageType =
  | "command"
  | "event"
  | "request"
  | "response"
  | "error"
  | "snapshot"
  | "heartbeat";

/**
 * Transport envelope per transport-envelope.schema.json.
 */
export interface TransportEnvelope {
  version: number;
  id: string;
  type: TransportMessageType;
  source: string;
  target?: string;
  topic: string;
  correlationId: string;
  timestamp: string;
  payload: Record<string, unknown>;
}

/**
 * Transport error payload.
 */
export interface TransportError {
  code: string;
  message: string;
  retryable?: boolean;
  details?: Record<string, unknown>;
}

/**
 * Reconnect policy configuration.
 */
export interface ReconnectPolicy {
  maxAttempts: number;
  initialDelayMs: number;
  maxDelayMs: number;
  multiplier: number;
}
