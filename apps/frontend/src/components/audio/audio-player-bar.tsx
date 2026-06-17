import { useEffect, useRef, useState } from "react";
import {
  MoreVertical,
  Pause,
  Play,
  Radio,
  Shuffle,
  SkipBack,
  SkipForward,
  Speaker,
  Square,
  Volume2,
} from "lucide-react";

import { Badge } from "@/components/ui/badge.js";
import { Button } from "@/components/ui/button.js";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu.js";
import { cn } from "@/lib/utils.js";
import type { AudioPlaybackView, PlaybackRemoteCommand } from "@/types/audio-types.js";

/**
 * Props for the Home Audio now-playing player bar.
 */
export interface AudioPlayerBarProps {
  /** Live playback view model. */
  playback: AudioPlaybackView;
  /** Sends a Shairport MQTT remote command. */
  onCommand: (command: PlaybackRemoteCommand) => void;
  /** Sets synced Hi-Fi zone volume (0–100). */
  onVolumeChange: (volume: number) => void;
}

/** Maximum plausible track length for display (8 hours). */
const MAX_DISPLAY_MS = 8 * 60 * 60 * 1000;

/**
 * Formats milliseconds as m:ss for the progress display.
 * @param ms - Duration in milliseconds.
 * @returns Formatted timestamp.
 */
function formatTime(ms: number, unknown = false): string {
  if (unknown || ms <= 0) {
    return "--:--";
  }
  const clamped = Math.max(0, Math.min(MAX_DISPLAY_MS, ms));
  const totalSeconds = Math.floor(clamped / 1000);
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;
  return `${minutes}:${seconds.toString().padStart(2, "0")}`;
}

/**
 * Styled range input for progress and volume controls.
 */
function PlayerSlider({
  value,
  max,
  disabled,
  accent,
  ariaLabel,
  onChange,
}: {
  value: number;
  max: number;
  disabled?: boolean;
  accent?: boolean;
  ariaLabel: string;
  onChange?: (value: number) => void;
}): React.JSX.Element {
  const clampedMax = Math.max(1, max);
  const clamped = Math.max(0, Math.min(clampedMax, value));
  const fillPercent = `${(clamped / clampedMax) * 100}%`;

  return (
    <input
      type="range"
      min={0}
      max={clampedMax}
      step={1}
      value={clamped}
      disabled={disabled}
      aria-label={ariaLabel}
      onChange={(event) => onChange?.(Number(event.target.value))}
      className={cn(
        "h-1.5 w-full cursor-pointer appearance-none rounded-full outline-none",
        "disabled:cursor-not-allowed disabled:opacity-50",
        "[&::-webkit-slider-runnable-track]:h-1.5 [&::-webkit-slider-runnable-track]:rounded-full",
        "[&::-webkit-slider-thumb]:mt-[-5px] [&::-webkit-slider-thumb]:h-3.5 [&::-webkit-slider-thumb]:w-3.5",
        "[&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:rounded-full",
        "[&::-webkit-slider-thumb]:border [&::-webkit-slider-thumb]:border-border [&::-webkit-slider-thumb]:bg-card",
        "[&::-webkit-slider-thumb]:shadow-sm",
        "[&::-moz-range-track]:h-1.5 [&::-moz-range-track]:rounded-full",
        "[&::-moz-range-thumb]:h-3.5 [&::-moz-range-thumb]:w-3.5 [&::-moz-range-thumb]:rounded-full",
        "[&::-moz-range-thumb]:border [&::-moz-range-thumb]:border-border [&::-moz-range-thumb]:bg-card"
      )}
      style={{
        background: `linear-gradient(to right, ${
          accent ? "var(--foreground)" : "var(--muted-foreground)"
        } ${fillPercent}, var(--muted) ${fillPercent})`,
      }}
    />
  );
}

/**
 * Now-playing transport bar shown above the Home Audio section tabs.
 */
export function AudioPlayerBar({
  playback,
  onCommand,
  onVolumeChange,
}: AudioPlayerBarProps): React.JSX.Element {
  const [coverFailed, setCoverFailed] = useState(false);
  const [localVolume, setLocalVolume] = useState(playback.volume);
  const [displayPositionMs, setDisplayPositionMs] = useState(playback.positionMs);
  const volumeDebounceRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    setLocalVolume(playback.volume);
  }, [playback.volume]);

  useEffect(() => {
    const elapsed = playback.playing ? Date.now() - playback.progressSyncedAt : 0;
    const extrapolated = playback.positionMs + Math.max(0, elapsed);
    const capped =
      playback.durationMs > 0
        ? Math.min(extrapolated, playback.durationMs)
        : extrapolated;
    setDisplayPositionMs(capped);
  }, [playback.positionMs, playback.durationMs, playback.playing, playback.progressSyncedAt]);

  useEffect(() => {
    if (!playback.playing) {
      return;
    }
    const timer = setInterval(() => {
      const elapsed = Date.now() - playback.progressSyncedAt;
      const extrapolated = playback.positionMs + Math.max(0, elapsed);
      const capped =
        playback.durationMs > 0
          ? Math.min(extrapolated, playback.durationMs)
          : extrapolated;
      setDisplayPositionMs(capped);
    }, 1000);
    return () => clearInterval(timer);
  }, [
    playback.playing,
    playback.positionMs,
    playback.durationMs,
    playback.progressSyncedAt,
  ]);

  useEffect(() => {
    return () => {
      if (volumeDebounceRef.current) {
        clearTimeout(volumeDebounceRef.current);
      }
    };
  }, []);

  const commitVolume = (volume: number): void => {
    if (volumeDebounceRef.current) {
      clearTimeout(volumeDebounceRef.current);
    }
    volumeDebounceRef.current = setTimeout(() => {
      onVolumeChange(volume);
    }, 200);
  };

  const positionMs = Math.min(displayPositionMs, MAX_DISPLAY_MS);
  const hasDuration = playback.durationMs > 0;
  const durationMs = hasDuration
    ? Math.min(playback.durationMs, MAX_DISPLAY_MS)
    : Math.max(positionMs, 1);
  const displayTitle = playback.track ?? "Now Playing";
  const displayArtist = playback.artist ?? "Unknown Artist";

  return (
    <section
      aria-label="Now playing"
      className="mb-6 rounded-2xl border border-border bg-card p-4 shadow-sm"
    >
      <div className="flex items-start gap-4">
        <div className="h-16 w-16 shrink-0 overflow-hidden rounded-xl bg-muted">
          {playback.coverUrl && !coverFailed ? (
            <img
              src={playback.coverUrl}
              alt=""
              className="h-full w-full object-cover"
              onError={() => setCoverFailed(true)}
            />
          ) : (
            <div className="flex h-full w-full items-center justify-center">
              <Radio className="h-6 w-6 text-muted-foreground" />
            </div>
          )}
        </div>

        <div className="min-w-0 flex-1">
          <p className="truncate text-base font-semibold text-foreground">{displayTitle}</p>
          <p className="truncate text-sm text-muted-foreground">{displayArtist}</p>
          {playback.sourceLabel ? (
            <Badge variant="secondary" className="mt-2 gap-1 rounded-full px-2 py-0.5 text-xs">
              <Radio className="h-3 w-3" />
              {playback.sourceLabel}
            </Badge>
          ) : null}
        </div>

        <div className="flex shrink-0 items-center gap-2">
          <div className="hidden items-center gap-1.5 rounded-full border border-border bg-muted/40 px-3 py-1.5 text-sm text-foreground sm:flex">
            <Speaker className="h-3.5 w-3.5 text-muted-foreground" />
            <span className="max-w-[8rem] truncate">{playback.zoneName}</span>
          </div>
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <Button
                type="button"
                variant="ghost"
                size="icon"
                className="h-8 w-8 rounded-full"
                aria-label="More playback options"
              >
                <MoreVertical className="h-4 w-4" />
              </Button>
            </DropdownMenuTrigger>
            <DropdownMenuContent align="end">
              <DropdownMenuItem onClick={() => onCommand("stop")}>
                <Square className="mr-2 h-4 w-4" />
                Stop
              </DropdownMenuItem>
            </DropdownMenuContent>
          </DropdownMenu>
        </div>
      </div>

      <div className="mt-4 flex items-center gap-3">
        <span className="w-10 shrink-0 text-xs tabular-nums text-muted-foreground">
          {formatTime(positionMs)}
        </span>
        <PlayerSlider
          value={positionMs}
          max={durationMs}
          disabled
          accent
          ariaLabel="Playback progress"
        />
        <span className="w-10 shrink-0 text-right text-xs tabular-nums text-muted-foreground">
          {formatTime(durationMs, !hasDuration)}
        </span>
      </div>

      <div className="mt-4 flex flex-wrap items-center justify-between gap-4">
        <Button
          type="button"
          variant="ghost"
          size="icon"
          className="h-9 w-9 rounded-full"
          aria-label="Shuffle"
          onClick={() => onCommand("shuffle_songs")}
        >
          <Shuffle className="h-4 w-4" />
        </Button>

        <div className="flex items-center gap-2 sm:gap-3">
          <Button
            type="button"
            variant="ghost"
            size="icon"
            className="h-10 w-10 rounded-full"
            aria-label="Previous track"
            onClick={() => onCommand("previtem")}
          >
            <SkipBack className="h-5 w-5" />
          </Button>
          <Button
            type="button"
            size="icon"
            className="h-12 w-12 rounded-full bg-foreground text-background hover:bg-foreground/90"
            aria-label={playback.playing ? "Pause" : "Play"}
            onClick={() => onCommand(playback.playing ? "pause" : "play")}
          >
            {playback.playing ? (
              <Pause className="h-5 w-5 fill-current" />
            ) : (
              <Play className="h-5 w-5 fill-current" />
            )}
          </Button>
          <Button
            type="button"
            variant="ghost"
            size="icon"
            className="h-10 w-10 rounded-full"
            aria-label="Next track"
            onClick={() => onCommand("nextitem")}
          >
            <SkipForward className="h-5 w-5" />
          </Button>
        </div>

        <div className="flex min-w-[8rem] flex-1 items-center justify-end gap-2 sm:min-w-[10rem] sm:flex-none">
          <Volume2 className="h-4 w-4 shrink-0 text-muted-foreground" />
          <PlayerSlider
            value={localVolume}
            max={100}
            ariaLabel="Volume"
            onChange={(volume) => {
              setLocalVolume(volume);
              commitVolume(volume);
            }}
          />
          <span className="w-7 shrink-0 text-right text-sm tabular-nums text-muted-foreground">
            {localVolume}
          </span>
        </div>
      </div>
    </section>
  );
}
