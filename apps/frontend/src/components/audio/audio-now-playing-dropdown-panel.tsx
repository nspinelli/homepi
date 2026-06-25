import { useMemo } from "react";

import {
  AudioCoverArt,
  AudioProgressBar,
  usePlaybackPosition,
} from "@/components/audio/audio-playback-ui.js";
import { ZonePowerButton } from "@/components/audio/zone-power-button.js";
import { ZoneVolumeKnob } from "@/components/audio/zone-volume-knob.js";
import { Badge } from "@/components/ui/badge.js";
import { getZoneActivityPriority } from "@/hooks/use-audio-module.js";
import { useAudioModule } from "@/hooks/audio-module-provider.js";
import { pickDisplayTitle } from "@/lib/audio-metadata-utils.js";
import { isZoneEnabled } from "@/lib/is-zone-enabled.js";
import { zoneCardVolume } from "@/lib/zone-card-volume.js";
import { cn } from "@/lib/utils.js";
import type { AudioPlaybackView } from "@/types/audio-types.js";

/**
 * Props for the now-playing dropdown panel body.
 */
export interface AudioNowPlayingDropdownPanelProps {
  /** Live playback view model. */
  playback: AudioPlaybackView;
}

/**
 * Now-playing panel for the header dropdown with track info, progress, and zone quick actions.
 * @param props - Playback data.
 */
export function AudioNowPlayingDropdownPanel({
  playback,
}: AudioNowPlayingDropdownPanelProps): React.JSX.Element {
  const {
    state,
    toggleZonePower,
    setZoneVolume,
    isZoneStreamedTo,
    isZoneSendingAudio,
  } = useAudioModule();
  const positionMs = usePlaybackPosition(playback);
  const title = pickDisplayTitle(playback);
  const artist = playback.artist?.trim();
  const album = playback.album?.trim();
  const clientPill = playback.clientName?.trim() || "AirPlay";

  const enabledZones = useMemo(() => {
    const zones = (state.snapshot?.zones ?? []).filter(isZoneEnabled);

    return [...zones].sort((zoneA, zoneB) => {
      const priorityA = getZoneActivityPriority(
        zoneA,
        isZoneSendingAudio(zoneA.zoneNumber),
        isZoneStreamedTo(zoneA.zoneNumber)
      );
      const priorityB = getZoneActivityPriority(
        zoneB,
        isZoneSendingAudio(zoneB.zoneNumber),
        isZoneStreamedTo(zoneB.zoneNumber)
      );

      if (priorityA !== priorityB) {
        return priorityA - priorityB;
      }

      return zoneA.zoneNumber - zoneB.zoneNumber;
    });
  }, [state.snapshot?.zones, isZoneSendingAudio, isZoneStreamedTo]);

  return (
    <div className="flex w-full flex-col">
      <div className="flex items-center gap-3">
        <AudioCoverArt coverUrl={playback.coverUrl} sizeClassName="h-24 w-24" />
        <div className="flex min-w-0 flex-1 flex-col justify-center gap-0.5">
          <p className="truncate text-sm font-semibold leading-tight text-foreground">{title}</p>
          {artist ? (
            <p className="truncate text-xs leading-tight text-muted-foreground">{artist}</p>
          ) : null}
          {album ? (
            <p className="truncate text-xs leading-tight text-muted-foreground">{album}</p>
          ) : null}
          <Badge variant="secondary" className="mt-1 w-fit max-w-full truncate text-[10px] font-normal">
            {clientPill}
          </Badge>
        </div>
      </div>

      <div className="mt-3 w-full">
        <AudioProgressBar positionMs={positionMs} durationMs={playback.durationMs} />
      </div>

      {enabledZones.length > 0 ? (
        <div className="mt-4 border-t border-border pt-3">
          <p className="mb-2 text-xs font-medium tracking-wider text-muted-foreground uppercase">
            Zones
          </p>
          <ul>
            {enabledZones.map((zone, index) => {
              const zoneName = zone.name?.trim() || `Zone ${zone.zoneNumber}`;
              const isOn = (zone.power ?? 0) === 1;
              const streamedTo = isZoneStreamedTo(zone.zoneNumber);
              const sendingAudio = isZoneSendingAudio(zone.zoneNumber);
              const accentActive = isOn || streamedTo || sendingAudio;
              const volume = zoneCardVolume(zone, streamedTo);

              return (
                <li
                  key={zone.zoneNumber}
                  className={cn(
                    "flex items-center gap-3 py-3",
                    index > 0 && "border-t border-border"
                  )}
                >
                  <ZonePowerButton
                    name={zoneName}
                    isEnabled
                    isOn={isOn}
                    isToggling={state.togglingPowerZone === zone.zoneNumber}
                    size="compact"
                    onToggle={() => {
                      void toggleZonePower(zone.zoneNumber);
                    }}
                  />
                  <div className="min-w-0 flex-1">
                    <p className="truncate text-sm font-medium text-foreground">{zoneName}</p>
                    <p
                      className={
                        accentActive
                          ? "text-xs text-zone-accent"
                          : "text-xs text-muted-foreground"
                      }
                    >
                      {sendingAudio ? "Playing" : isOn ? "On" : streamedTo ? "Streaming" : "Off"}
                    </p>
                  </div>
                  <ZoneVolumeKnob
                    value={volume}
                    isActive={accentActive}
                    ariaLabel={`${zoneName} volume`}
                    onChange={(nextVolume) => {
                      void setZoneVolume(zone.zoneNumber, nextVolume);
                    }}
                  />
                </li>
              );
            })}
          </ul>
        </div>
      ) : null}
    </div>
  );
}
