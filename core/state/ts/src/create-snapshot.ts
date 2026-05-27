import { randomUUID } from "node:crypto";
import type { StateSnapshot } from "./state-types.js";

/**
 * Creates a schema-compliant state snapshot.
 * @param params - Snapshot fields.
 * @returns State snapshot.
 */
export function createSnapshot(params: {
  owner: string;
  topic: string;
  state: Record<string, unknown>;
  snapshotId?: string;
  createdAt?: string;
}): StateSnapshot {
  return {
    snapshotId: params.snapshotId ?? randomUUID(),
    owner: params.owner,
    topic: params.topic,
    createdAt: params.createdAt ?? new Date().toISOString(),
    state: params.state,
  };
}
