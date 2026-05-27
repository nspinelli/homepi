/**
 * Metric type per metric-sample.schema.json.
 */
export type MetricType = "counter" | "gauge" | "timer";

/**
 * Metric sample per metric-sample.schema.json.
 */
export interface MetricSample {
  /** Metric name. */
  name: string;
  /** Metric type. */
  type: MetricType;
  /** Metric value. */
  value: number;
  /** ISO8601 UTC timestamp. */
  timestamp: string;
  /** Optional metric labels. */
  labels?: Record<string, string>;
}
