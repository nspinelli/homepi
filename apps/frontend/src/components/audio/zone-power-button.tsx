import { Play, Square } from "lucide-react";

import { Button } from "@/components/ui/button.js";
import { cn } from "@/lib/utils.js";

/**
 * Props for the zone play/stop power toggle button.
 */
export interface ZonePowerButtonProps {
  /** Zone display name for aria labels. */
  name: string;
  /** Whether the zone is enabled in the controller. */
  isEnabled: boolean;
  /** Whether zone power is on. */
  isOn: boolean;
  /** Whether a power toggle request is in flight. */
  isToggling?: boolean;
  /** Turns the zone on or off. */
  onToggle: () => void;
  /** Optional size variant. */
  size?: "default" | "compact";
}

/**
 * Circular play/stop control shared by zone cards and now-playing quick actions.
 * @param props - Zone power button options.
 */
export function ZonePowerButton({
  name,
  isEnabled,
  isOn,
  isToggling = false,
  onToggle,
  size = "default",
}: ZonePowerButtonProps): React.JSX.Element {
  const accentActive = isEnabled && isOn;
  const dimensionClass = size === "compact" ? "h-9 w-9" : "h-11 w-11";
  const iconClass = size === "compact" ? "h-3.5 w-3.5" : "h-4 w-4";

  return (
    <Button
      type="button"
      variant="outline"
      size="icon"
      disabled={!isEnabled || isToggling}
      aria-label={isOn ? `Turn off ${name}` : `Turn on ${name}`}
      className={cn(
        "shrink-0 rounded-full border-2 bg-transparent",
        dimensionClass,
        accentActive
          ? "border-zone-accent hover:bg-zone-accent/10"
          : "border-muted-foreground/40 hover:bg-muted/50"
      )}
      onClick={(event) => {
        event.stopPropagation();
        onToggle();
      }}
    >
      {isOn ? (
        <Square
          className={cn(
            "fill-current",
            iconClass,
            accentActive ? "text-zone-accent" : "text-muted-foreground"
          )}
        />
      ) : (
        <Play
          className={cn(
            "fill-current",
            iconClass,
            isEnabled ? "text-muted-foreground" : "text-muted-foreground/50"
          )}
        />
      )}
    </Button>
  );
}
