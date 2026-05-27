import { createMetricSample } from "./create-metric-sample.js";

/**
 * In-process gauge metric helper.
 */
export class Gauge {
  /**
   * @param name - Metric name.
   * @param initialValue - Initial gauge value.
   * @param labels - Optional metric labels.
   */
  constructor(
    private readonly name: string,
    private value = 0,
    private readonly labels?: Record<string, string>
  ) {}

  /**
   * Sets the gauge value.
   * @param value - New gauge value.
   */
  set(value: number): void {
    this.value = value;
  }

  /**
   * Returns the current gauge value.
   * @returns Gauge value.
   */
  getValue(): number {
    return this.value;
  }

  /**
   * Creates a metric sample for the current gauge value.
   * @returns Metric sample.
   */
  sample(): ReturnType<typeof createMetricSample> {
    return createMetricSample({
      name: this.name,
      type: "gauge",
      value: this.value,
      labels: this.labels,
    });
  }
}
