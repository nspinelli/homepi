import { createMetricSample } from "./create-metric-sample.js";

/**
 * In-process timer metric helper.
 */
export class Timer {
  private elapsedMs = 0;

  /**
   * @param name - Metric name.
   * @param labels - Optional metric labels.
   */
  constructor(
    private readonly name: string,
    private readonly labels?: Record<string, string>
  ) {}

  /**
   * Records an elapsed duration in milliseconds.
   * @param durationMs - Duration to record.
   */
  record(durationMs: number): void {
    this.elapsedMs = durationMs;
  }

  /**
   * Returns the last recorded duration.
   * @returns Duration in milliseconds.
   */
  getValue(): number {
    return this.elapsedMs;
  }

  /**
   * Creates a metric sample for the last recorded duration.
   * @returns Metric sample.
   */
  sample(): ReturnType<typeof createMetricSample> {
    return createMetricSample({
      name: this.name,
      type: "timer",
      value: this.elapsedMs,
      labels: this.labels,
    });
  }
}
