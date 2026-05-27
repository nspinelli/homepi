import { describe, expect, it } from "vitest";
import stateSnapshotExample from "../../examples/state-snapshot.example.json" with { type: "json" };
import stateDeltaExample from "../../examples/state-delta.example.json" with { type: "json" };
import stateSnapshotSchema from "../../schema/state-snapshot.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { applyDelta } from "./apply-delta.js";
import type { StateDelta, StateSnapshot } from "./state-types.js";

describe("StateSnapshot", () => {
  it("validates the documented example", () => {
    const result = validateAgainstSchema(stateSnapshotSchema, stateSnapshotExample);
    expect(result.valid).toBe(true);
  });

  it("applies delta changes to snapshot state", () => {
    const snapshot = stateSnapshotExample as StateSnapshot;
    const delta = stateDeltaExample as StateDelta;
    const nextState = applyDelta(snapshot, delta);
    expect((nextState.zones as unknown[])[4]).toEqual({ power: true });
  });
});
