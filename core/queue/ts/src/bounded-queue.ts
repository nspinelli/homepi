import type { QueueItem } from "./queue-types.js";

/**
 * In-memory bounded queue with backpressure on enqueue.
 */
export class BoundedQueue {
  private readonly items: QueueItem[] = [];

  /**
   * @param capacity - Maximum queue depth.
   */
  constructor(private readonly capacity: number) {
    if (capacity < 1) {
      throw new Error("BoundedQueue capacity must be at least 1");
    }
  }

  /**
   * Current queue depth.
   */
  get depth(): number {
    return this.items.length;
  }

  /**
   * Maximum queue capacity.
   */
  get maxCapacity(): number {
    return this.capacity;
  }

  /**
   * Whether the queue is at capacity.
   */
  get isFull(): boolean {
    return this.items.length >= this.capacity;
  }

  /**
   * Attempts to enqueue an item; returns false when full.
   * @param item - Queue item to enqueue.
   * @returns True when accepted.
   */
  enqueue(item: QueueItem): boolean {
    if (this.isFull) {
      return false;
    }
    this.items.push(item);
    return true;
  }

  /**
   * Removes and returns the oldest queued item.
   * @returns Oldest item or undefined when empty.
   */
  dequeue(): QueueItem | undefined {
    return this.items.shift();
  }

  /**
   * Clears all queued items.
   */
  clear(): void {
    this.items.length = 0;
  }
}
