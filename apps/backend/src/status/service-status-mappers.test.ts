import { describe, expect, it } from "vitest";

import {
  mapHifiSerialFromPayload,
  mapHifiSerialStatus,
  mapLifecycleEventToStatus,
  mapPcmRouterFromDacState,
  mapPcmRouterFromPayload,
  mapSystemdServiceStatus,
  mapUsbDevicesFromPayload,
  mapUsbDevicesStatus,
} from "./service-status-mappers.js";

describe("service-status-mappers", () => {
  it("maps USB devices status from reachable and degraded flags", () => {
    expect(mapUsbDevicesStatus(false, false)).toBe("offline");
    expect(mapUsbDevicesStatus(true, true)).toBe("degraded");
    expect(mapUsbDevicesStatus(true, false)).toBe("healthy");
  });

  it("maps HiFi serial health snapshot", () => {
    expect(
      mapHifiSerialStatus({
        lifecycle: "running",
        connected: false,
        serialPath: null,
        serialAssigned: false,
        syncInProgress: false,
        degraded: false,
        lastFullSyncAt: null,
        queueDepth: 0,
      })
    ).toBe("offline");
    expect(
      mapHifiSerialStatus({
        lifecycle: "running",
        connected: true,
        serialPath: "/dev/vHifi",
        serialAssigned: true,
        syncInProgress: true,
        degraded: false,
        lastFullSyncAt: null,
        queueDepth: 0,
      })
    ).toBe("degraded");
  });

  it("maps systemd active states", () => {
    expect(mapSystemdServiceStatus("active")).toBe("healthy");
    expect(mapSystemdServiceStatus("activating")).toBe("degraded");
    expect(mapSystemdServiceStatus("inactive")).toBe("offline");
  });

  it("maps service health payloads", () => {
    expect(mapUsbDevicesFromPayload({ status: "degraded" })).toBe("degraded");
    expect(mapHifiSerialFromPayload({ status: "healthy" })).toBe("healthy");
    expect(
      mapPcmRouterFromPayload({ status: "running", audioActive: true }, "health")
    ).toBe("healthy");
  });

  it("maps PCM router DAC states", () => {
    expect(mapPcmRouterFromDacState("open")).toBe("healthy");
    expect(mapPcmRouterFromDacState("idle")).toBe("healthy");
    expect(mapPcmRouterFromDacState("DAC_OPEN")).toBe("healthy");
    expect(mapPcmRouterFromDacState("DAC_IDLE")).toBe("healthy");
    expect(mapPcmRouterFromDacState("unassigned")).toBe("degraded");
    expect(mapPcmRouterFromDacState("unavailable")).toBe("degraded");
    expect(mapPcmRouterFromDacState("DAC_UNASSIGNED")).toBe("degraded");
    expect(mapPcmRouterFromDacState("DAC_UNAVAILABLE")).toBe("degraded");
    expect(
      mapPcmRouterFromPayload({ dacState: "idle" }, "pcm_router_snapshot")
    ).toBe("healthy");
  });

  it("maps journal lifecycle events", () => {
    expect(mapLifecycleEventToStatus("service_started")).toBe("healthy");
    expect(mapLifecycleEventToStatus("service_stopped")).toBe("offline");
  });
});
