import { beforeEach, describe, expect, it, vi } from "vitest";

import {
  getDefaultUserSettings,
  readUserSettings,
  resolveThemePreference,
  USER_SETTINGS_STORAGE_KEY,
  writeUserSettings,
} from "./user-settings-storage.js";

describe("user-settings-storage", () => {
  beforeEach(() => {
    window.localStorage.clear();
  });

  it("returns defaults when storage is empty", () => {
    expect(readUserSettings()).toEqual(getDefaultUserSettings());
  });

  it("persists and reads appearance theme", () => {
    writeUserSettings({
      appearance: { theme: "light" },
    });

    expect(window.localStorage.getItem(USER_SETTINGS_STORAGE_KEY)).toContain('"light"');
    expect(readUserSettings().appearance.theme).toBe("light");
  });

  it("resolves system theme from prefers-color-scheme", () => {
    vi.spyOn(window, "matchMedia").mockReturnValue({
      matches: true,
      media: "(prefers-color-scheme: dark)",
      onchange: null,
      addEventListener: vi.fn(),
      removeEventListener: vi.fn(),
      addListener: vi.fn(),
      removeListener: vi.fn(),
      dispatchEvent: vi.fn(),
    } as MediaQueryList);

    expect(resolveThemePreference("system")).toBe("dark");
    expect(resolveThemePreference("light")).toBe("light");
  });
});
