export type {
  StateDelta,
  StateDeltaChange,
  StateDeltaOperation,
  StateEnvelope,
  StateSnapshot,
} from "./state-types.js";
export { createSnapshot } from "./create-snapshot.js";
export { applyDelta } from "./apply-delta.js";
