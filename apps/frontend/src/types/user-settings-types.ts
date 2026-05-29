/**
 * Theme preference stored in user settings.
 */
export type ThemePreference = "light" | "dark" | "system";

/**
 * Appearance-related user preferences.
 */
export interface AppearanceSettings {
  /** Color scheme preference. */
  theme: ThemePreference;
}

/**
 * Persisted HomePi user settings for the web app.
 */
export interface UserSettings {
  /** Visual preferences. */
  appearance: AppearanceSettings;
}

/**
 * Resolved theme applied to the document root.
 */
export type ResolvedTheme = "light" | "dark";
