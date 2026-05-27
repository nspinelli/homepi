/**
 * State delta operation per state-delta.schema.json.
 */
export type StateDeltaOperation = "set" | "remove" | "replace";

/**
 * Single state change entry.
 */
export interface StateDeltaChange {
  /** JSON pointer-style path. */
  path: string;
  /** Change operation. */
  op: StateDeltaOperation;
  /** Value for set/replace operations. */
  value?: unknown;
}

/**
 * State envelope shape per state-envelope.schema.json.
 */
export interface StateEnvelope {
  /** Schema version. */
  version: number;
  /** Authoritative owner service. */
  owner: string;
  /** State topic. */
  topic: string;
  /** ISO8601 UTC timestamp. */
  timestamp: string;
  /** Current state object. */
  state: Record<string, unknown>;
}

/**
 * State snapshot shape per state-snapshot.schema.json.
 */
export interface StateSnapshot {
  /** Unique snapshot identifier. */
  snapshotId: string;
  /** Authoritative owner service. */
  owner: string;
  /** State topic. */
  topic: string;
  /** ISO8601 UTC creation timestamp. */
  createdAt: string;
  /** Snapshot state object. */
  state: Record<string, unknown>;
}

/**
 * State delta shape per state-delta.schema.json.
 */
export interface StateDelta {
  /** Unique delta identifier. */
  deltaId: string;
  /** Authoritative owner service. */
  owner: string;
  /** State topic. */
  topic: string;
  /** ISO8601 UTC timestamp. */
  timestamp: string;
  /** Ordered state changes. */
  changes: StateDeltaChange[];
}
