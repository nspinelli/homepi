import { describe, expect, it } from "vitest";

import {
  buildHealthEvidenceMessage,
  buildModuleCapabilities,
  resolveServiceRollupStatus,
  type ServiceHealthEntry,
} from "./health-snapshot-builder.js";
import type { ServiceRegistry, ServiceRegistryEntry } from "@homepi/core-service-registry";

const testRegistry: ServiceRegistry = {
  version: 1,
  modules: [
    {
      id: "audio",
      displayName: "Home Audio",
      facadeService: "homepi-audio",
      commandSocket: "/run/homepi/audio/audio.sock",
      icon: "/audio-controller.png",
      capabilities: ["zone-control", "paging"],
    },
  ],
  services: [
    {
      name: "homepi-hifi-serial",
      module: "audio",
      unit: "homepi-hifi-serial.service",
      role: "hardware-controller",
      commandSocket: "/run/homepi/audio/hifi-serial.sock",
      critical: true,
      capabilitiesAffected: ["zone-control"],
      userFacingFailureCategory: "audio",
    },
    {
      name: "homepi-audio-paging",
      module: "audio",
      unit: "homepi-audio-paging.service",
      role: "hardware-controller",
      commandSocket: "/run/homepi/audio/paging.sock",
      critical: false,
      capabilitiesAffected: ["paging"],
      userFacingFailureCategory: "audio",
    },
  ],
};

describe("buildHealthEvidenceMessage", () => {
  it("returns the existing user message when present", () => {
    const message = buildHealthEvidenceMessage({
      service: "homepi-pcm-router",
      module: "audio",
      status: "healthy",
      userMessage: "homepi-pcm-router socket is reachable.",
      lastUpdated: new Date().toISOString(),
    });

    expect(message).toBe("homepi-pcm-router socket is reachable.");
  });

  it("builds layered evidence for healthy services without a user message", () => {
    const message = buildHealthEvidenceMessage({
      service: "homepi-hifi-serial",
      module: "audio",
      status: "healthy",
      process: "active",
      readiness: "ready",
      domain: "ready",
      lastUpdated: new Date().toISOString(),
    });

    expect(message).toBe("Process active · Command socket ready · Domain checks passing");
  });
});

describe("resolveServiceRollupStatus", () => {
  const socketlessEntry: ServiceRegistryEntry = {
    name: "homepi-nqptp",
    module: "audio",
    unit: "homepi-nqptp.service",
    role: "hardware-controller",
    critical: false,
    capabilitiesAffected: ["airplay"],
    userFacingFailureCategory: "audio",
  };

  it("marks active socketless services healthy", () => {
    expect(
      resolveServiceRollupStatus(socketlessEntry, "active", "unknown", "unknown")
    ).toBe("healthy");
  });

  it("keeps inactive socketless services offline", () => {
    expect(
      resolveServiceRollupStatus(socketlessEntry, "inactive", "unknown", "unknown")
    ).toBe("offline");
  });

  it("does not override socket-probed degraded services", () => {
    const probedEntry: ServiceRegistryEntry = {
      ...socketlessEntry,
      name: "homepi-hifi-serial",
      commandSocket: "/run/homepi/audio/hifi-serial.sock",
    };

    expect(
      resolveServiceRollupStatus(probedEntry, "active", "not_ready", "unknown")
    ).toBe("degraded");
  });
});

describe("buildModuleCapabilities", () => {
  it("includes healthy evidence for each capability from related services", () => {
    const serviceHealth: ServiceHealthEntry[] = [
      {
        service: "homepi-hifi-serial",
        module: "audio",
        status: "healthy",
        process: "active",
        readiness: "ready",
        domain: "ready",
        userMessage: "Zone controller responding.",
        lastUpdated: "2026-06-28T18:00:00.000Z",
      },
      {
        service: "homepi-audio-paging",
        module: "audio",
        status: "healthy",
        process: "active",
        readiness: "ready",
        domain: "unknown",
        lastUpdated: "2026-06-28T18:00:00.000Z",
      },
    ];

    const capabilities = buildModuleCapabilities(
      testRegistry.modules[0]!,
      serviceHealth,
      testRegistry
    );

    expect(capabilities).toEqual([
      expect.objectContaining({
        id: "zone-control",
        status: "healthy",
        userMessage: "Zone controller responding.",
      }),
      expect.objectContaining({
        id: "paging",
        status: "healthy",
        userMessage: "Process active · Command socket ready · Responding on command socket",
      }),
    ]);
  });
});
