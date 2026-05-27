import { describe, expect, it } from "vitest";
import queueItemExample from "../../examples/queue-item.example.json" with { type: "json" };
import queueItemSchema from "../../schema/queue-item.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { BoundedQueue } from "./bounded-queue.js";
import { computeRetryDelay } from "./compute-retry-delay.js";
import { createQueueItem } from "./create-queue-item.js";

describe("QueueItem", () => {
  it("validates the documented example", () => {
    const result = validateAgainstSchema(queueItemSchema, queueItemExample);
    expect(result.valid).toBe(true);
  });

  it("creates schema-valid queue items", () => {
    const item = createQueueItem({
      id: "queue-001",
      type: "serial_command",
      priority: 50,
      correlationId: "cmd-001",
      createdAt: "2026-05-27T16:00:00.000Z",
      payload: { command: "example" },
    });
    const result = validateAgainstSchema(queueItemSchema, item);
    expect(result.valid).toBe(true);
  });

  it("applies bounded queue backpressure", () => {
    const queue = new BoundedQueue(1);
    const first = createQueueItem({
      id: "queue-001",
      type: "serial_command",
      payload: { command: "first" },
    });
    const second = createQueueItem({
      id: "queue-002",
      type: "serial_command",
      payload: { command: "second" },
    });

    expect(queue.enqueue(first)).toBe(true);
    expect(queue.enqueue(second)).toBe(false);
    expect(queue.dequeue()?.id).toBe("queue-001");
  });

  it("computes retry delays from policy", () => {
    expect(computeRetryDelay({ maxAttempts: 3, backoffMs: 100 }, 1)).toBe(100);
    expect(
      computeRetryDelay({ maxAttempts: 3, backoffMs: 100, strategy: "exponential" }, 2)
    ).toBe(200);
  });
});
