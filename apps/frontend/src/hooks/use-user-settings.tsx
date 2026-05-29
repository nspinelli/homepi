import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from "react";

import {
  applyThemePreference,
  getDefaultUserSettings,
  readUserSettings,
  resolveThemePreference,
  writeUserSettings,
} from "@/lib/user-settings-storage.js";
import type {
  ResolvedTheme,
  ThemePreference,
  UserSettings,
} from "@/types/user-settings-types.js";

interface UserSettingsContextValue {
  /** Current persisted user settings. */
  settings: UserSettings;
  /** Resolved theme currently applied to the document. */
  resolvedTheme: ResolvedTheme;
  /** Whether client settings have been hydrated. */
  mounted: boolean;
  /**
   * Updates the appearance theme preference.
   * @param theme - New theme preference.
   */
  setThemePreference: (theme: ThemePreference) => void;
}

const UserSettingsContext = createContext<UserSettingsContextValue | null>(null);

/**
 * Provides persisted user settings and applies appearance preferences to the document.
 * @param props - Provider props.
 * @param props.children - Child tree.
 * @returns Provider element.
 */
export function UserSettingsProvider({ children }: { children: ReactNode }): React.JSX.Element {
  const [settings, setSettings] = useState<UserSettings>(getDefaultUserSettings);
  const [mounted, setMounted] = useState(false);
  const [resolvedTheme, setResolvedTheme] = useState<ResolvedTheme>("dark");

  useEffect(() => {
    const stored = readUserSettings();
    setSettings(stored);
    applyThemePreference(stored.appearance.theme);
    setResolvedTheme(resolveThemePreference(stored.appearance.theme));
    setMounted(true);
  }, []);

  useEffect(() => {
    if (!mounted) {
      return;
    }

    applyThemePreference(settings.appearance.theme);
    setResolvedTheme(resolveThemePreference(settings.appearance.theme));
    writeUserSettings(settings);
  }, [mounted, settings]);

  useEffect(() => {
    if (!mounted || settings.appearance.theme !== "system") {
      return;
    }

    const media = window.matchMedia("(prefers-color-scheme: dark)");
    const handleChange = (): void => {
      applyThemePreference("system");
      setResolvedTheme(resolveThemePreference("system"));
    };

    media.addEventListener("change", handleChange);
    return () => media.removeEventListener("change", handleChange);
  }, [mounted, settings.appearance.theme]);

  const setThemePreference = useCallback((theme: ThemePreference) => {
    setSettings((current) => ({
      ...current,
      appearance: {
        ...current.appearance,
        theme,
      },
    }));
  }, []);

  const value = useMemo(
    () => ({
      settings,
      resolvedTheme,
      mounted,
      setThemePreference,
    }),
    [settings, resolvedTheme, mounted, setThemePreference]
  );

  return (
    <UserSettingsContext.Provider value={value}>{children}</UserSettingsContext.Provider>
  );
}

/**
 * Reads persisted user settings from context.
 * @returns User settings context value.
 */
export function useUserSettings(): UserSettingsContextValue {
  const context = useContext(UserSettingsContext);
  if (!context) {
    throw new Error("useUserSettings must be used within UserSettingsProvider");
  }
  return context;
}
