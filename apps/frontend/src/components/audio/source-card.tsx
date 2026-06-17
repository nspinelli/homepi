import { Airplay, Pencil, Radio } from "lucide-react";

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
  onEdit: (id: number) => void;
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
}: SourceCardProps): React.JSX.Element {
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
              <h4 className="truncate font-semibold text-foreground">{name}</h4>
              <p className="text-sm text-muted-foreground">Source {id}</p>
            </div>
          </div>
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
        </div>

        <div className="mt-4 flex flex-wrap items-center gap-2 text-sm">
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
          {isAirplay ? (
            <span className="rounded-full bg-zone-accent/15 px-2.5 py-0.5 text-xs font-medium text-zone-accent">
              AirPlay
            </span>
          ) : null}
        </div>

        {(displayLine || inputGain !== undefined) && (
          <div className="mt-4 border-t border-border pt-3 text-sm text-muted-foreground">
            {displayLine ? <p className="truncate">{displayLine}</p> : null}
            {inputGain !== undefined ? (
              <p className={displayLine ? "mt-1" : ""}>Input gain: {inputGain}</p>
            ) : null}
          </div>
        )}
      </CardContent>
    </Card>
  );
}
