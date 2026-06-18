import { Airplay, Pencil, Radio } from "lucide-react";

import { ZoneEditSlider } from "@/components/audio/zone-edit-slider.js";
import {
  SOURCE_INPUT_GAIN_MAX,
  SOURCE_INPUT_GAIN_MIN,
} from "@/components/audio/source-input-gain-limits.js";
import { Button } from "@/components/ui/button.js";
import { Card, CardContent } from "@/components/ui/card.js";
import { cn } from "@/lib/utils.js";

/**
 * Props for a source status card in the sources grid.
 */
export interface SourceCardProps {
  /** Source display name. */
  name: string;
  /** Source number 1-8. */
  id: number;
  /** Whether the source is enabled in the controller. */
  isEnabled: boolean;
  /** Whether this source is the designated AirPlay slot. */
  isAirplay: boolean;
  /** Optional input gain value. */
  inputGain?: number;
  /** Optional display line text. */
  displayLine?: string;
  /** Opens the source settings editor. */
  onEdit?: (id: number) => void;
  /** Hides the pencil edit control (e.g. in editor preview). */
  hideEditButton?: boolean;
  /** Replaces the title with an inline editable field. */
  editableName?: boolean;
  /** Called when the name field changes in editable mode. */
  onNameChange?: (name: string) => void;
  /** Allows toggling enabled by tapping the status badge. */
  interactiveEnabled?: boolean;
  /** Called when the enabled badge is toggled in interactive mode. */
  onEnabledChange?: (enabled: boolean) => void;
  /** Called when input gain changes in interactive mode. */
  onInputGainChange?: (inputGain: number) => void;
}

/**
 * Source card with AirPlay badge, status, and edit affordance.
 */
export function SourceCard({
  name,
  id,
  isEnabled,
  isAirplay,
  inputGain,
  displayLine,
  onEdit,
  hideEditButton = false,
  editableName = false,
  onNameChange,
  interactiveEnabled = false,
  onEnabledChange,
  onInputGainChange,
}: SourceCardProps): React.JSX.Element {
  const showEditButton = !hideEditButton && onEdit !== undefined;
  const gainValue = Math.max(
    SOURCE_INPUT_GAIN_MIN,
    Math.min(SOURCE_INPUT_GAIN_MAX, inputGain ?? SOURCE_INPUT_GAIN_MIN)
  );
  const interactiveInputGain = onInputGainChange !== undefined;

  return (
    <Card
      className={cn(
        "min-w-0 border border-border shadow-sm transition-all",
        !isEnabled && "opacity-60"
      )}
    >
      <CardContent className="p-4">
        <div className="flex items-start justify-between gap-3">
          <div className="flex min-w-0 flex-1 items-center gap-3">
            <div
              className={cn(
                "flex h-11 w-11 shrink-0 items-center justify-center rounded-full border-2",
                isAirplay
                  ? "border-zone-accent bg-zone-accent/10"
                  : isEnabled
                    ? "border-muted-foreground/40 bg-muted/30"
                    : "border-muted-foreground/30 bg-muted/20"
              )}
            >
              {isAirplay ? (
                <Airplay className="h-5 w-5 text-zone-accent" aria-hidden />
              ) : (
                <Radio
                  className={cn(
                    "h-5 w-5",
                    isEnabled ? "text-muted-foreground" : "text-muted-foreground/50"
                  )}
                  aria-hidden
                />
              )}
            </div>
            <div className="min-w-0">
              {editableName ? (
                <input
                  type="text"
                  value={name}
                  onChange={(event) => onNameChange?.(event.target.value)}
                  className="w-full min-w-0 truncate border-0 bg-transparent p-0 font-semibold text-foreground outline-none focus:ring-0"
                  aria-label="Source name"
                />
              ) : (
                <h4 className="truncate font-semibold text-foreground">{name}</h4>
              )}
              <p className="text-sm text-muted-foreground">Source {id}</p>
            </div>
          </div>
          {showEditButton ? (
          <Button
            variant="ghost"
            size="icon"
            className="h-8 w-8 shrink-0 rounded-full bg-muted/60"
            onClick={(event) => {
              event.stopPropagation();
              onEdit(id);
            }}
          >
            <Pencil className="h-4 w-4 text-muted-foreground" />
            <span className="sr-only">Edit source</span>
          </Button>
          ) : null}
        </div>

        <div className="mt-4 flex flex-wrap items-center gap-2 text-sm">
          {interactiveEnabled && onEnabledChange ? (
            <button
              type="button"
              className={cn(
                "rounded-full px-2.5 py-0.5 text-xs font-medium transition-opacity hover:opacity-80",
                isEnabled
                  ? "bg-emerald-500/15 text-emerald-700 dark:text-emerald-400"
                  : "bg-muted text-muted-foreground"
              )}
              onClick={() => onEnabledChange(!isEnabled)}
            >
              {isEnabled ? "Enabled" : "Disabled"}
            </button>
          ) : (
            <span
              className={cn(
                "rounded-full px-2.5 py-0.5 text-xs font-medium",
                isEnabled
                  ? "bg-emerald-500/15 text-emerald-700 dark:text-emerald-400"
                  : "bg-muted text-muted-foreground"
              )}
            >
              {isEnabled ? "Enabled" : "Disabled"}
            </span>
          )}
          {isAirplay ? (
            <span className="rounded-full bg-zone-accent/15 px-2.5 py-0.5 text-xs font-medium text-zone-accent">
              AirPlay
            </span>
          ) : null}
        </div>

        {displayLine ? (
          <div className="mt-4 border-t border-border pt-3 text-sm text-muted-foreground">
            <p className="truncate">{displayLine}</p>
          </div>
        ) : null}

        <div className={cn("border-t border-border pt-3", displayLine ? "mt-3" : "mt-4")}>
          <div className="mb-2 flex items-center justify-between gap-2">
            <span className="text-sm font-medium text-foreground">Input gain</span>
            <span className="text-sm text-muted-foreground">{gainValue}</span>
          </div>
          <ZoneEditSlider
            min={SOURCE_INPUT_GAIN_MIN}
            max={SOURCE_INPUT_GAIN_MAX}
            value={gainValue}
            disabled={!interactiveInputGain}
            ariaLabel="Source input gain"
            onChange={(value) => onInputGainChange?.(value)}
          />
        </div>
      </CardContent>
    </Card>
  );
}
