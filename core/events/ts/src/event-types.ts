/**
 * Event envelope shape per event-envelope.schema.json.
 */
export interface EventEnvelope {
  /** Schema version. */
  version: number;
  /** Unique event identifier. */
  id: string;
  /** Publishing service name. */
  source: string;
  /** Dot-separated topic path. */
  topic: string;
  /** Stable snake_case event name. */
  event: string;
  /** Operation trace identifier. */
  correlationId: string;
  /** ISO8601 UTC timestamp. */
  timestamp: string;
  /** Event-specific payload. */
  payload: Record<string, unknown>;
}
