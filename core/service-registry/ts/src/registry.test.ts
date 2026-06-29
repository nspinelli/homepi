import { describe, expect, it } from "vitest";

import { findModule, loadServiceRegistry, servicesForModule } from "./load-registry.js";

describe("loadServiceRegistry", () => {
  it("loads bundled registry with two client modules", () => {
    const registry = loadServiceRegistry();

    expect(registry.version).toBe(1);
    expect(registry.modules).toHaveLength(2);
    expect(findModule(registry, "audio")?.icon).toBe("/audio-controller.png");
    expect(findModule(registry, "contact-sensors")?.icon).toBe("/contact-sensors.png");
  });

  it("lists audio internal services", () => {
    const registry = loadServiceRegistry();
    const audioServices = servicesForModule(registry, "audio");

    expect(audioServices.some((entry) => entry.name === "homepi-pcm-router")).toBe(true);
  });
});
