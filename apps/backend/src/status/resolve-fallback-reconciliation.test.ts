import { describe, expect, it } from "vitest";

import type { ServiceConfig } from "@homepi/core-config";

import { FALLBACK_RECONCILIATION_INTERVAL_MS } from "./fallback-reconciliation.js";
import { resolveFallbackReconciliation } from "./resolve-fallback-reconciliation.js";

const baseConfig: ServiceConfig = {
  service: "homepi-backend",
  environment: "test",
  logging: { level: "INFO" },
  runtime: {
    paths: {
      homepiRoot: "/opt/homepi",
      runtimeDir: "/opt/homepi/runtime",
      generatedDir: "/opt/homepi/runtime/generated",
      stateDir: "/opt/homepi/runtime/state",
      socketDir: "/run/homepi",
    },
  },
};

describe("resolveFallbackReconciliation", () => {
  it("uses defaults when status section is omitted", () => {
    const resolved = resolveFallbackReconciliation(baseConfig);
    expect(resolved.enabled).toBe(true);
    expect(resolved.intervalMs).toBe(FALLBACK_RECONCILIATION_INTERVAL_MS);
  });

  it("respects custom interval and disabled flag", () => {
    const resolved = resolveFallbackReconciliation({
      ...baseConfig,
      status: {
        fallbackReconciliation: { enabled: false, intervalMs: 180_000 },
      },
    });
    expect(resolved.enabled).toBe(false);
    expect(resolved.intervalMs).toBe(180_000);
  });
});
