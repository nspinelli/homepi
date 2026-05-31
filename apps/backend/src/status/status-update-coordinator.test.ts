import { describe, expect, it, vi } from "vitest";

import { EventBroadcaster } from "../event-broadcaster.js";
import { SystemStatusStore } from "../system-status-store.js";
import { StatusUpdateCoordinator } from "./status-update-coordinator.js";

describe("StatusUpdateCoordinator", () => {
  it("broadcasts only when service fields change", () => {
    const logger = {
      info: vi.fn(),
      warn: vi.fn(),
      debug: vi.fn(),
    };
    const store = new SystemStatusStore({
      backend: "healthy",
      config: "loaded",
      logging: "active",
      runtime: "running",
      transport: "ready",
      events: "ready",
      state: "ready",
      api: "ready",
      usbDevices: "offline",
      hifiSerial: "offline",
      nqptp: "offline",
      metadata: "offline",
      pcmRouter: "offline",
      shairport: "offline",
      uptimeMs: 0,
      cpuTempC: null,
      lastEventAt: null,
    });
    const broadcaster = new EventBroadcaster(logger as never, () => store.getStatus());
    const broadcastSpy = vi.spyOn(broadcaster, "broadcastStatusDelta");
    const coordinator = new StatusUpdateCoordinator({ statusStore: store, broadcaster });

    coordinator.patchAndBroadcast({ usbDevices: "healthy" }, "test");
    expect(broadcastSpy).toHaveBeenCalledTimes(1);

    coordinator.patchAndBroadcast({ usbDevices: "healthy" }, "test");
    expect(broadcastSpy).toHaveBeenCalledTimes(1);

    coordinator.patchAndBroadcast({ usbDevices: "degraded" }, "test");
    expect(broadcastSpy).toHaveBeenCalledTimes(2);
  });
});
