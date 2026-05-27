import type { MetricSample, MetricType } from "./metric-types.js";

/**
 * Creates a schema-compliant metric sample.
 * @param params - Metric sample fields.
 * @returns Metric sample.
 */
export function createMetricSample(params: {
  name: string;
  type: MetricType;
  value: number;
  timestamp?: string;
  labels?: Record<string, string>;
}): MetricSample {
  return {
    name: params.name,
    type: params.type,
    value: params.value,
    timestamp: params.timestamp ?? new Date().toISOString(),
    labels: params.labels,
  };
}
