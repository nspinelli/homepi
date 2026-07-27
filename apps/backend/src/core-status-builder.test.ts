import { describe, expect, it } from "vitest";

import { buildHealthReportFromSnapshot } from "./core-status-builder.js";
import type { SystemHealthSnapshot } from "./health/health-client.js";

describe("buildHealthReportFromSnapshot", () => {
  it("treats planned modules as warn so the backend is not marked failed", () => {
    const snapshot: SystemHealthSnapshot = {
      checkedAt: new Date().toISOString(),
      healthServiceReachable: true,
      modules: [
        {
          module: "audio",
          displayName: "Home Audio",
          icon: "/audio-controller.png",
          status: "healthy",
          capabilities: [],
          lastUpdated: new Date().toISOString(),
        },
        {
          module: "contact-sensors",
          displayName: "Contact Sensors",
          icon: "/sensors-module.png?v=3",
          status: "offline",
          planned: true,
          userMessage: "homepi-sensors is planned but not installed yet.",
          capabilities: [],
          lastUpdated: new Date().toISOString(),
        },
      ],
      platform: [],
      services: [],
    };

    const report = buildHealthReportFromSnapshot(
      { service: "homepi-backend" } as never,
      snapshot
    );

    expect(report.status).toBe("degraded");
    expect(report.checks.find((check) => check.name === "contact-sensors")?.status).toBe("warn");
  });
});
