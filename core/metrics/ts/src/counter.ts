import { createMetricSample } from "./create-metric-sample.js";

/**
 * In-process counter metric helper.
 */
export class Counter {
  private value = 0;

  /**
   * @param name - Metric name.
   * @param labels - Optional metric labels.
   */
  constructor(
    private readonly name: string,
    private readonly labels?: Record<string, string>
  ) {}

  /**
   * Increments the counter.
   * @param amount - Increment amount.
   */
  increment(amount = 1): void {
    this.value += amount;
  }

  /**
   * Returns the current counter value.
   * @returns Counter value.
   */
  getValue(): number {
    return this.value;
  }

  /**
   * Creates a metric sample for the current counter value.
   * @returns Metric sample.
   */
  sample(): ReturnType<typeof createMetricSample> {
    return createMetricSample({
      name: this.name,
      type: "counter",
      value: this.value,
      labels: this.labels,
    });
  }
}
