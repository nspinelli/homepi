/**
 * Simple bounded buffer for transport backpressure.
 */
export class BoundedBuffer<T> {
  private readonly maxSize: number;
  private readonly items: T[] = [];

  /**
   * @param maxSize - Maximum buffered items.
   */
  constructor(maxSize: number) {
    this.maxSize = maxSize;
  }

  /**
   * Current buffer length.
   */
  get size(): number {
    return this.items.length;
  }

  /**
   * Whether the buffer is at capacity.
   */
  get isFull(): boolean {
    return this.items.length >= this.maxSize;
  }

  /**
   * Attempts to push an item; returns false when full.
   * @param item - Item to buffer.
   * @returns True when accepted.
   */
  tryPush(item: T): boolean {
    if (this.isFull) {
      return false;
    }
    this.items.push(item);
    return true;
  }

  /**
   * Removes and returns the oldest item.
   * @returns Oldest item or undefined when empty.
   */
  shift(): T | undefined {
    return this.items.shift();
  }

  /**
   * Clears the buffer.
   */
  clear(): void {
    this.items.length = 0;
  }
}
