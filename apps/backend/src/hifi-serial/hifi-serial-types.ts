/**
 * HiFi serial dashboard status.
 */
export type HifiSerialStatus = "healthy" | "degraded" | "offline";

/**
 * Health snapshot from homepi-hifi-serial.
 */
export interface HifiSerialHealth {
  /** Service lifecycle state. */
  lifecycle: string;
  /** Serial port open and communicating. */
  connected: boolean;
  /** Active serial device path or null. */
  serialPath: string | null;
  /** USB serial role assigned. */
  serialAssigned: boolean;
  /** Full sync in progress. */
  syncInProgress: boolean;
  /** Degraded operation flag. */
  degraded: boolean;
  /** ISO timestamp of last full sync or null. */
  lastFullSyncAt: string | null;
  /** Pending command queue depth. */
  queueDepth: number;
}

/**
 * Socket API response envelope from homepi-hifi-serial.
 */
export interface HifiSocketResponse<T = Record<string, unknown>> {
  /** Success flag. */
  ok: boolean;
  /** Correlation identifier. */
  correlationId: string;
  /** Response payload when ok. */
  data?: T;
  /** Error details when not ok. */
  error?: {
    code: string;
    message: string;
  };
}
