import type { ApiError, ApiResponse } from "./api-types.js";

/**
 * Creates a successful API response envelope.
 * @param params - Response fields.
 * @returns API response.
 */
export function createSuccessResponse(params: {
  correlationId: string;
  data?: Record<string, unknown>;
  timestamp?: string;
}): ApiResponse {
  return {
    ok: true,
    correlationId: params.correlationId,
    timestamp: params.timestamp ?? new Date().toISOString(),
    data: params.data,
  };
}

/**
 * Creates an error API response envelope.
 * @param params - Response fields.
 * @returns API response.
 */
export function createErrorResponse(params: {
  correlationId: string;
  error: ApiError;
  timestamp?: string;
}): ApiResponse {
  return {
    ok: false,
    correlationId: params.correlationId,
    timestamp: params.timestamp ?? new Date().toISOString(),
    error: params.error,
  };
}
