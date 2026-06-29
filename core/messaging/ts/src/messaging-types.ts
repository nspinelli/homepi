/**
 * Structured error returned in socket responses.
 */
export interface MessagingError {
  /** Machine-readable error code. */
  code: string;
  /** Severity level. */
  severity: "info" | "warning" | "error" | "critical";
  /** User-facing message. */
  userMessage: string;
  /** Developer-facing message. */
  developerMessage: string;
  /** Originating service name. */
  service: string;
  /** Whether the error may resolve without restart. */
  recoverable: boolean;
  /** Whether retrying the command may succeed. */
  retryable: boolean;
  /** Optional structured details. */
  details?: Record<string, unknown>;
  /** Optional correlation id. */
  correlationId?: string;
}

/**
 * v1 socket request envelope.
 */
export interface SocketRequest {
  /** Protocol version. */
  v: 1;
  /** Request id. */
  id: string;
  /** Calling service name. */
  source: string;
  /** Target service name. */
  target: string;
  /** Command name. */
  command: string;
  /** Correlation id for tracing. */
  correlationId: string;
  /** Optional command payload. */
  payload?: Record<string, unknown>;
}

/**
 * v1 socket response envelope.
 */
export interface SocketResponse {
  /** Protocol version. */
  v: 1;
  /** Matching request id. */
  id: string;
  /** Whether the command succeeded. */
  ok: boolean;
  /** Success result payload. */
  result?: Record<string, unknown>;
  /** Structured error when ok is false. */
  error?: MessagingError;
}

/**
 * Broker event envelope.
 */
export interface BrokerEvent {
  /** Protocol version. */
  v: 1;
  /** Event id. */
  id: string;
  /** ISO timestamp. */
  ts: string;
  /** Topic in homepi.module.entity.event form. */
  topic: string;
  /** Publishing service. */
  source: string;
  /** Correlation id. */
  correlationId: string;
  /** Event severity. */
  severity: "debug" | "info" | "warning" | "error";
  /** Event payload. */
  payload: Record<string, unknown>;
  /** False hides the event from the status page activity log. */
  uiVisible?: boolean;
}

/**
 * Legacy native RPC request ({method, correlationId}).
 */
export interface LegacyRpcRequest {
  /** RPC method name. */
  method: string;
  /** Correlation id. */
  correlationId: string;
  [key: string]: unknown;
}

/**
 * Legacy native RPC response.
 */
export interface LegacyRpcResponse {
  /** Whether the call succeeded. */
  ok: boolean;
  /** Response data. */
  data?: Record<string, unknown>;
  /** Error message. */
  error?: string;
  /** Interleaved event line marker. */
  event?: string;
}
