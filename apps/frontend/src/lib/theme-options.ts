import { Monitor, Moon, Sun } from "lucide-react";

import type { ThemePreference } from "@/types/user-settings-types.js";

/**
 * Theme preference option for appearance pickers.
 */
export interface ThemeOption {
  /** Stored preference value. */
  value: ThemePreference;
  /** Visible label. */
  label: string;
  /** Lucide icon for the option. */
  icon: typeof Sun;
}

/** Appearance options shown in header and settings pickers. */
export const THEME_OPTIONS: ThemeOption[] = [
  { value: "light", label: "Light", icon: Sun },
  { value: "dark", label: "Dark", icon: Moon },
  { value: "system", label: "System", icon: Monitor },
];
