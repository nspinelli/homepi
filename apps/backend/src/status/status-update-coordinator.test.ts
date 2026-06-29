import { describe, expect, it, vi } from "vitest";

import { EventBroadcaster } from "../event-broadcaster.js";
import { SystemStatusStore } from "../system-status-store.js";
import { StatusUpdateCoordinator } from "./status-update-coordinator.js";

describe("StatusUpdateCoordinator", () => {
  it("broadcasts only when host metric fields change", () => {
    const logger = {
      info: vi.fn(),
      warn: vi.fn(),
      debug: vi.fn(),
    };
    const store = new SystemStatusStore({
      uptimeMs: 0,
      cpuTempC: null,
      lastEventAt: null,
    });
    const broadcaster = new EventBroadcaster(logger as never, () => store.getStatus());
    const broadcastSpy = vi.spyOn(broadcaster, "broadcastStatusDelta");
    const coordinator = new StatusUpdateCoordinator({ statusStore: store, broadcaster });

    coordinator.patchAndBroadcast({ cpuTempC: 42.1 }, "test");
    expect(broadcastSpy).toHaveBeenCalledTimes(1);

    coordinator.patchAndBroadcast({ cpuTempC: 42.1 }, "test");
    expect(broadcastSpy).toHaveBeenCalledTimes(1);

    coordinator.patchAndBroadcast({ cpuTempC: 43.0 }, "test");
    expect(broadcastSpy).toHaveBeenCalledTimes(2);
  });
});
