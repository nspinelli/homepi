/**
 * API error payload per api-error.schema.json.
 */
export interface ApiError {
  /** Stable uppercase error code. */
  code: string;
  /** Human-readable error message. */
  message: string;
  /** Optional structured error details. */
  details?: Record<string, unknown>;
}

/**
 * API response envelope per api-response.schema.json.
 */
export interface ApiResponse {
  /** Whether the request succeeded. */
  ok: boolean;
  /** Request correlation identifier. */
  correlationId: string;
  /** ISO8601 UTC timestamp. */
  timestamp: string;
  /** Success payload. */
  data?: Record<string, unknown>;
  /** Error payload when ok is false. */
  error?: ApiError;
}

/**
 * Pagination payload per pagination.schema.json.
 */
export interface Pagination {
  /** Page size limit. */
  limit: number;
  /** Result offset. */
  offset: number;
  /** Total available records. */
  total: number;
}
