import { Check } from "lucide-react";

import { Button } from "@/components/ui/button.js";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu.js";
import { useUserSettings } from "@/hooks/use-user-settings.js";
import { THEME_OPTIONS } from "@/lib/theme-options.js";

/**
 * Header icon button for selecting light, dark, or system appearance.
 */
export function ThemeHeaderButton(): React.JSX.Element {
  const { settings, mounted, setThemePreference } = useUserSettings();
  const selected =
    THEME_OPTIONS.find((option) => option.value === settings.appearance.theme) ??
    THEME_OPTIONS[2];
  const SelectedIcon = selected.icon;

  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <Button
          type="button"
          variant="ghost"
          size="icon"
          className="shrink-0 px-2"
          aria-label={`Appearance: ${selected.label}`}
        >
          <SelectedIcon className="size-4" aria-hidden />
        </Button>
      </DropdownMenuTrigger>
      <DropdownMenuContent align="end" className="min-w-[10rem]">
        {THEME_OPTIONS.map((option) => {
          const Icon = option.icon;
          const isSelected = mounted && settings.appearance.theme === option.value;

          return (
            <DropdownMenuItem
              key={option.value}
              onClick={() => setThemePreference(option.value)}
              className="justify-between"
            >
              <span className="flex items-center gap-2">
                <Icon className="size-4" aria-hidden />
                {option.label}
              </span>
              {isSelected ? <Check className="size-4" aria-hidden /> : null}
            </DropdownMenuItem>
          );
        })}
      </DropdownMenuContent>
    </DropdownMenu>
  );
}
