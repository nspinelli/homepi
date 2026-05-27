/**
 * Sliding-window rate limiter keyed by event name.
 */
export class LogRateLimiter {
  private readonly maxPerWindow: number;
  private readonly windowMs: number;
  private readonly buckets = new Map<string, number[]>();

  /**
   * @param maxPerWindow - Maximum log emissions per key per window.
   * @param windowMs - Window duration in milliseconds.
   */
  constructor(maxPerWindow: number, windowMs: number) {
    this.maxPerWindow = maxPerWindow;
    this.windowMs = windowMs;
  }

  /**
   * Returns whether a log for the given key should be emitted.
   * @param key - Rate limit key (typically event name).
   * @returns True if the log should be emitted.
   */
  shouldEmit(key: string): boolean {
    const now = Date.now();
    const timestamps = this.buckets.get(key) ?? [];
    const recent = timestamps.filter((t) => now - t < this.windowMs);

    if (recent.length >= this.maxPerWindow) {
      this.buckets.set(key, recent);
      return false;
    }

    recent.push(now);
    this.buckets.set(key, recent);
    return true;
  }

  /**
   * Clears all rate limit state.
   */
  reset(): void {
    this.buckets.clear();
  }
}
