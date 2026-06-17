import { Airplay, MonitorUp, Pencil, Speaker } from "lucide-react";

import { ZonePowerButton } from "@/components/audio/zone-power-button.js";
import { ZoneVolumeSlider } from "@/components/audio/zone-volume-slider.js";
import { Button } from "@/components/ui/button.js";
import { Card, CardContent } from "@/components/ui/card.js";
import { cn } from "@/lib/utils.js";

/**
 * Props for a zone status card in the audio zones grid.
 */
export interface ZoneCardProps {
  /** Zone display name. */
  name: string;
  /** Zone number 1-16. */
  id: number;
  /** Whether the zone is enabled in the controller. */
  isEnabled: boolean;
  /** Whether zone power is on. */
  isOn?: boolean;
  /** Current zone volume 0-100. */
  volume?: number;
  /** Active source number for the zone. */
  sourceNumber?: number;
  /** Whether the zone is in the PCM router active stack (streamed to). */
  isStreamedTo?: boolean;
  /** Whether this zone is sending audio to the DAC. */
  isSendingAudio?: boolean;
  /** Whether a power toggle request is in flight. */
  isTogglingPower?: boolean;
  /** Turns the zone on or off. */
  onTogglePower: (id: number) => void;
  /** Sets zone volume 0-100. */
  onVolumeChange: (id: number, volume: number) => void;
  /** Opens the zone settings editor. */
  onEdit: (id: number) => void;
}

/**
 * Zone card with play/stop control, volume, source status, and AirPlay indicator.
 */
export function ZoneCard({
  name,
  id,
  isEnabled,
  isOn = false,
  volume = 0,
  sourceNumber,
  isStreamedTo = false,
  isSendingAudio = false,
  isTogglingPower = false,
  onTogglePower,
  onVolumeChange,
  onEdit,
}: ZoneCardProps): React.JSX.Element {
  const accentActive = isEnabled && isOn;
  const sourceLabel =
    sourceNumber !== undefined ? `Source ${sourceNumber}` : "No source";
  const sourceStatus = isOn ? "Active" : "Available";

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
            <ZonePowerButton
              name={name}
              isEnabled={isEnabled}
              isOn={isOn}
              isToggling={isTogglingPower}
              onToggle={() => onTogglePower(id)}
            />
            <div className="min-w-0">
              <h4 className="truncate font-semibold text-foreground">{name}</h4>
              <p className="text-sm text-muted-foreground">Zone {id}</p>
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
            <span className="sr-only">Edit zone</span>
          </Button>
        </div>

        <div className="mt-4 flex min-w-0 items-center gap-2 sm:gap-3">
          <Speaker className="h-4 w-4 shrink-0 text-muted-foreground" />
          <div className="min-w-0 flex-1">
            <ZoneVolumeSlider
              value={volume}
              isActive={accentActive || isStreamedTo}
              disabled={!isEnabled}
              onChange={(nextVolume) => onVolumeChange(id, nextVolume)}
            />
          </div>
          <span className="w-9 shrink-0 text-right text-sm text-muted-foreground sm:w-10">
            {volume}%
          </span>
        </div>

        <div className="mt-4 flex items-center justify-between border-t border-border pt-3">
          <div className="flex min-w-0 items-center gap-2 text-sm text-muted-foreground">
            <MonitorUp className="h-4 w-4 shrink-0" />
            <span className="truncate">
              {sourceLabel} • {sourceStatus}
            </span>
          </div>
          {isStreamedTo ? (
            <Airplay
              className={cn(
                "h-4 w-4 shrink-0",
                isSendingAudio ? "text-zone-accent" : "text-muted-foreground"
              )}
              aria-label={
                isSendingAudio
                  ? "Sending audio to DAC"
                  : "AirPlay session active"
              }
            />
          ) : null}
        </div>
      </CardContent>
    </Card>
  );
}
