import { describe, expect, it, vi } from "vitest";

import type { UsbDevicesClient } from "./usb-devices-client.js";
import { UsbDevicesRoutes } from "./usb-devices-routes.js";

describe("UsbDevicesRoutes", () => {
  it("matches usb device API paths", () => {
    const routes = new UsbDevicesRoutes({
      client: {} as UsbDevicesClient,
      logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn(), debug: vi.fn() },
    });
    expect(routes.matches("/api/usb-devices")).toBe(true);
    expect(routes.matches("/api/usb-devices/assignments")).toBe(true);
    expect(routes.matches("/api/health")).toBe(false);
  });
});
