import type { StateDelta, StateDeltaChange, StateSnapshot } from "./state-types.js";

type MutableState = Record<string, unknown> | unknown[];

/**
 * Parses a JSON pointer-style path into segments.
 * @param path - Path such as `/zones/4/power`.
 * @returns Path segments.
 */
function parsePath(path: string): string[] {
  return path.split("/").filter((segment) => segment.length > 0);
}

/**
 * Returns whether a path segment is a numeric array index.
 * @param segment - Path segment.
 * @returns True when segment is numeric.
 */
function isArrayIndex(segment: string): boolean {
  return /^\d+$/.test(segment);
}

/**
 * Applies a single change to a mutable state tree.
 * @param state - State tree to mutate.
 * @param change - Delta change entry.
 */
function applyChange(state: MutableState, change: StateDeltaChange): void {
  const segments = parsePath(change.path);
  if (segments.length === 0) {
    return;
  }

  let current: MutableState = state;
  for (let index = 0; index < segments.length - 1; index += 1) {
    const segment = segments[index]!;
    const next = Array.isArray(current) ? current[Number(segment)] : current[segment];

    if (typeof next !== "object" || next === null) {
      const created: Record<string, unknown> = {};
      if (Array.isArray(current) && isArrayIndex(segment)) {
        current[Number(segment)] = created;
      } else if (!Array.isArray(current)) {
        current[segment] = created;
      }
      current = created;
      continue;
    }

    current = next as MutableState;
  }

  const leaf = segments[segments.length - 1]!;
  if (Array.isArray(current) && isArrayIndex(leaf)) {
    const arrayIndex = Number(leaf);
    if (change.op === "remove") {
      current.splice(arrayIndex, 1);
      return;
    }
    current[arrayIndex] = change.value;
    return;
  }

  if (change.op === "remove") {
    delete (current as Record<string, unknown>)[leaf];
    return;
  }

  (current as Record<string, unknown>)[leaf] = change.value;
}

/**
 * Applies delta changes to snapshot state and returns a new state object.
 * @param snapshot - Authoritative snapshot.
 * @param delta - Delta to apply.
 * @returns Updated state object.
 */
export function applyDelta(
  snapshot: StateSnapshot,
  delta: StateDelta
): Record<string, unknown> {
  const nextState = structuredClone(snapshot.state) as Record<string, unknown>;
  for (const change of delta.changes) {
    applyChange(nextState, change);
  }
  return nextState;
}
