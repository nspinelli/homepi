import { createCorrelationId, createRequestId } from "./correlation.js";
import type { MessagingError, SocketRequest, SocketResponse } from "./messaging-types.js";

/**
 * Input for creating a socket request.
 */
export interface CreateRequestInput {
  /** Calling service name. */
  source: string;
  /** Target service name. */
  target: string;
  /** Command name. */
  command: string;
  /** Optional correlation id. */
  correlationId?: string;
  /** Optional payload. */
  payload?: Record<string, unknown>;
}

/**
 * Creates a v1 socket request envelope.
 * @param input - Request fields.
 * @returns Socket request object.
 */
export function createRequest(input: CreateRequestInput): SocketRequest {
  return {
    v: 1,
    id: createRequestId(),
    source: input.source,
    target: input.target,
    command: input.command,
    correlationId: input.correlationId ?? createCorrelationId(input.command),
    ...(input.payload ? { payload: input.payload } : {}),
  };
}

/**
 * Creates a success socket response.
 * @param request - Original request.
 * @param result - Result payload.
 * @returns Socket response.
 */
export function createSuccessResponse(
  request: Pick<SocketRequest, "id">,
  result: Record<string, unknown>
): SocketResponse {
  return {
    v: 1,
    id: request.id,
    ok: true,
    result,
  };
}

/**
 * Creates a failure socket response.
 * @param request - Original request.
 * @param error - Structured error.
 * @returns Socket response.
 */
export function createErrorResponse(
  request: Pick<SocketRequest, "id">,
  error: MessagingError
): SocketResponse {
  return {
    v: 1,
    id: request.id,
    ok: false,
    error,
  };
}

/**
 * Builds a structured messaging error.
 * @param input - Error fields.
 * @returns Messaging error object.
 */
export function createMessagingError(
  input: Omit<MessagingError, "severity"> & { severity?: MessagingError["severity"] }
): MessagingError {
  return {
    severity: input.severity ?? "error",
    code: input.code,
    userMessage: input.userMessage,
    developerMessage: input.developerMessage,
    service: input.service,
    recoverable: input.recoverable,
    retryable: input.retryable,
    ...(input.details ? { details: input.details } : {}),
    ...(input.correlationId ? { correlationId: input.correlationId } : {}),
  };
}

/**
 * Parses a JSON line as a socket response.
 * @param line - NDJSON line.
 * @returns Parsed response or null.
 */
export function parseResponseLine(line: string): SocketResponse | null {
  try {
    const parsed = JSON.parse(line) as SocketResponse;
    if (parsed.v === 1 && typeof parsed.id === "string" && typeof parsed.ok === "boolean") {
      return parsed;
    }
    return null;
  } catch {
    return null;
  }
}

/**
 * Parses a JSON line as a legacy RPC response.
 * @param line - NDJSON line.
 * @returns Parsed legacy response or null.
 */
export function parseLegacyResponseLine(line: string): Record<string, unknown> | null {
  try {
    return JSON.parse(line) as Record<string, unknown>;
  } catch {
    return null;
  }
}
