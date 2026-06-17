import { useEffect, useState } from "react";

import { cn } from "@/lib/utils.js";

/** Maximum plausible track length for display (8 hours). */
export const MAX_DISPLAY_MS = 8 * 60 * 60 * 1000;

/**
 * Formats milliseconds as m:ss for the progress display.
 * @param ms - Duration in milliseconds.
 * @param unknown - When true, renders a placeholder.
 * @returns Formatted timestamp.
 */
export function formatPlaybackTime(ms: number, unknown = false): string {
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
 * Inputs for extrapolating live playback position.
 */
export interface PlaybackPositionInput {
  /** Whether transport is actively playing. */
  playing: boolean;
  /** Last synced position in milliseconds. */
  positionMs: number;
  /** Track duration in milliseconds. */
  durationMs: number;
  /** Wall-clock ms when positionMs was last synced. */
  progressSyncedAt: number;
}

/**
 * Extrapolates the current playback position from the last sync point.
 * @param playback - Playback timing fields.
 * @returns Current position capped by duration when known.
 */
export function usePlaybackPosition(playback: PlaybackPositionInput): number {
  const [displayPositionMs, setDisplayPositionMs] = useState(playback.positionMs);

  useEffect(() => {
    const elapsed = playback.playing ? Date.now() - playback.progressSyncedAt : 0;
    const extrapolated = playback.positionMs + Math.max(0, elapsed);
    const capped =
      playback.durationMs > 0
        ? Math.min(extrapolated, playback.durationMs)
        : extrapolated;
    setDisplayPositionMs(capped);
  }, [
    playback.positionMs,
    playback.durationMs,
    playback.playing,
    playback.progressSyncedAt,
  ]);

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

  return Math.min(displayPositionMs, MAX_DISPLAY_MS);
}

/**
 * Props for the read-only playback progress bar.
 */
export interface AudioProgressBarProps {
  /** Current position in milliseconds. */
  positionMs: number;
  /** Track duration in milliseconds. */
  durationMs: number;
}

/**
 * Read-only progress bar with elapsed and remaining timestamps.
 * @param props - Progress values.
 */
export function AudioProgressBar({
  positionMs,
  durationMs,
}: AudioProgressBarProps): React.JSX.Element {
  const hasDuration = durationMs > 0;
  const displayDuration = hasDuration
    ? Math.min(durationMs, MAX_DISPLAY_MS)
    : positionMs > 0
      ? Math.min(Math.max(durationMs, positionMs + 120_000), MAX_DISPLAY_MS)
      : 0;
  const hasDisplayDuration = displayDuration > 0;
  const remainingMs = hasDisplayDuration ? Math.max(0, displayDuration - positionMs) : 0;
  const fillPercent = hasDisplayDuration
    ? `${Math.min(100, (positionMs / displayDuration) * 100)}%`
    : undefined;

  return (
    <div className="flex items-center gap-3">
      <span className="w-10 shrink-0 text-xs tabular-nums text-muted-foreground">
        {formatPlaybackTime(positionMs)}
      </span>
      <div
        className="h-1.5 flex-1 overflow-hidden rounded-full bg-muted"
        role="progressbar"
        aria-valuenow={positionMs}
        aria-valuemin={0}
        aria-valuemax={hasDisplayDuration ? displayDuration : undefined}
      >
        {hasDisplayDuration ? (
          <div
            className="h-full rounded-full bg-foreground transition-[width] duration-300"
            style={{ width: fillPercent }}
          />
        ) : (
          <div className="h-full w-1/3 animate-pulse rounded-full bg-foreground/40" />
        )}
      </div>
      <span className="w-10 shrink-0 text-right text-xs tabular-nums text-muted-foreground">
        {formatPlaybackTime(remainingMs, !hasDuration)}
      </span>
    </div>
  );
}

/**
 * Props for a circular cover-art thumbnail.
 */
export interface AudioCoverArtProps {
  /** Cover image URL. */
  coverUrl?: string;
  /** Square size class. */
  sizeClassName?: string;
  /** Optional image class override. */
  className?: string;
}

/**
 * Album artwork with a muted fallback icon area.
 * @param props - Cover art options.
 */
export function AudioCoverArt({
  coverUrl,
  sizeClassName = "h-28 w-28",
  className,
}: AudioCoverArtProps): React.JSX.Element {
  const [coverFailed, setCoverFailed] = useState(false);

  return (
    <div
      className={cn(
        "shrink-0 overflow-hidden rounded-xl bg-muted shadow-sm",
        sizeClassName,
        className
      )}
    >
      {coverUrl && !coverFailed ? (
        <img
          src={coverUrl}
          alt=""
          className="h-full w-full object-cover"
          onError={() => setCoverFailed(true)}
        />
      ) : (
        <div className="flex h-full w-full items-center justify-center bg-muted">
          <div className="h-10 w-10 rounded-full bg-muted-foreground/15" aria-hidden />
        </div>
      )}
    </div>
  );
}
