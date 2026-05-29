import type { ThemePreference, UserSettings } from "../types/user-settings-types.js";

/** localStorage key for persisted user settings. */
export const USER_SETTINGS_STORAGE_KEY = "homepi:user-settings";

const DEFAULT_USER_SETTINGS: UserSettings = {
  appearance: {
    theme: "system",
  },
};

/**
 * Returns the default user settings snapshot.
 * @returns Default settings object.
 */
export function getDefaultUserSettings(): UserSettings {
  return structuredClone(DEFAULT_USER_SETTINGS);
}

/**
 * Reads user settings from localStorage.
 * @returns Parsed settings or defaults when missing or invalid.
 */
export function readUserSettings(): UserSettings {
  if (typeof window === "undefined") {
    return getDefaultUserSettings();
  }

  try {
    const raw = window.localStorage.getItem(USER_SETTINGS_STORAGE_KEY);
    if (!raw) {
      return getDefaultUserSettings();
    }

    const parsed = JSON.parse(raw) as Partial<UserSettings>;
    const theme = parsed.appearance?.theme;

    if (theme === "light" || theme === "dark" || theme === "system") {
      return {
        appearance: { theme },
      };
    }
  } catch {
    // Fall through to defaults.
  }

  return getDefaultUserSettings();
}

/**
 * Persists user settings to localStorage.
 * @param settings - Settings snapshot to store.
 */
export function writeUserSettings(settings: UserSettings): void {
  window.localStorage.setItem(USER_SETTINGS_STORAGE_KEY, JSON.stringify(settings));
}

/**
 * Resolves a theme preference to a concrete light or dark value.
 * @param theme - Stored theme preference.
 * @returns Resolved theme for DOM class application.
 */
export function resolveThemePreference(theme: ThemePreference): "light" | "dark" {
  if (theme === "system") {
    return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
  }
  return theme;
}

/**
 * Applies the resolved theme class on the document root element.
 * @param theme - Stored theme preference.
 */
export function applyThemePreference(theme: ThemePreference): void {
  const resolved = resolveThemePreference(theme);
  document.documentElement.classList.toggle("dark", resolved === "dark");
}
