import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { ArrowLeft, Check, Monitor, Moon, Sun } from "lucide-react";

import { AudioConfigurationCard } from "@/components/audio-configuration-card.js";
import { Button } from "@/components/ui/button.js";
import { useUserSettings } from "@/hooks/use-user-settings.js";
import type { ThemePreference } from "@/types/user-settings-types.js";

const THEME_OPTIONS: Array<{
  value: ThemePreference;
  label: string;
  description: string;
  icon: typeof Sun;
}> = [
  {
    value: "light",
    label: "Light",
    description: "Light grey background with dark text",
    icon: Sun,
  },
  {
    value: "dark",
    label: "Dark",
    description: "Dark grey background with light text",
    icon: Moon,
  },
  {
    value: "system",
    label: "System",
    description: "Follows your device settings",
    icon: Monitor,
  },
];

/**
 * Settings route with appearance preferences persisted as user settings.
 */
export function SettingsPage(): React.JSX.Element {
  const { settings, mounted, setThemePreference } = useUserSettings();
  const [hydrated, setHydrated] = useState(false);

  useEffect(() => {
    setHydrated(true);
  }, []);

  const showSelection = mounted && hydrated;

  return (
    <main className="mx-auto max-w-4xl px-4 py-6">
      <div className="mb-6">
        <Button variant="ghost" size="sm" className="mb-4 gap-2 text-muted-foreground" asChild>
          <Link to="/">
            <ArrowLeft className="size-4" />
            Back
          </Link>
        </Button>
        <h1 className="text-2xl font-semibold text-foreground">Settings</h1>
        <p className="mt-1 text-sm text-muted-foreground">Manage your HomePi preferences</p>
      </div>

      <div className="mb-6">
        <AudioConfigurationCard />
      </div>

      <div className="rounded-lg border border-border bg-card">
        <div className="border-b border-border px-6 py-4">
          <h2 className="font-medium text-card-foreground">Appearance</h2>
          <p className="mt-0.5 text-sm text-muted-foreground">
            Customize how HomePi looks on your device
          </p>
        </div>

        <div className="p-6">
          <div className="grid gap-3">
            {THEME_OPTIONS.map((option) => {
              const Icon = option.icon;
              const isSelected = showSelection && settings.appearance.theme === option.value;

              return (
                <button
                  key={option.value}
                  type="button"
                  onClick={() => setThemePreference(option.value)}
                  className={`flex items-center gap-4 rounded-lg border p-4 text-left transition-colors ${
                    isSelected
                      ? "border-foreground bg-secondary/50"
                      : "border-border hover:border-muted-foreground/50 hover:bg-secondary/30"
                  }`}
                >
                  <div
                    className={`flex size-10 items-center justify-center rounded-lg ${
                      isSelected ? "bg-foreground text-background" : "bg-secondary text-foreground"
                    }`}
                  >
                    <Icon className="size-5" />
                  </div>
                  <div className="flex-1">
                    <p className="font-medium text-foreground">{option.label}</p>
                    <p className="text-sm text-muted-foreground">{option.description}</p>
                  </div>
                  {isSelected ? <Check className="size-5 text-foreground" /> : null}
                </button>
              );
            })}
          </div>
        </div>
      </div>
    </main>
  );
}
