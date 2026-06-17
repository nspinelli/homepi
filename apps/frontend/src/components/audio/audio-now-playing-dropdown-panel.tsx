import {
  AudioCoverArt,
  AudioProgressBar,
  usePlaybackPosition,
} from "@/components/audio/audio-playback-ui.js";
import type { AudioPlaybackView } from "@/types/audio-types.js";

/**
 * Props for the now-playing dropdown panel body.
 */
export interface AudioNowPlayingDropdownPanelProps {
  /** Live playback view model. */
  playback: AudioPlaybackView;
}

/**
 * Minimal now-playing panel for the header dropdown.
 * @param props - Playback data.
 */
export function AudioNowPlayingDropdownPanel({
  playback,
}: AudioNowPlayingDropdownPanelProps): React.JSX.Element {
  const positionMs = usePlaybackPosition(playback);
  const title = playback.track?.trim() || "Now Playing";
  const artist = playback.artist?.trim();
  const album = playback.album?.trim();

  return (
    <div className="flex flex-col items-center text-center">
      <AudioCoverArt coverUrl={playback.coverUrl} sizeClassName="h-24 w-24" />
      <h2 className="mt-3 w-full truncate text-sm font-semibold text-foreground">{title}</h2>
      {artist ? (
        <p className="mt-0.5 w-full truncate text-xs text-muted-foreground">{artist}</p>
      ) : null}
      {album ? (
        <p className="mt-0.5 w-full truncate text-[11px] text-muted-foreground/80">{album}</p>
      ) : null}
      <div className="mt-3 w-full">
        <AudioProgressBar positionMs={positionMs} durationMs={playback.durationMs} />
      </div>
    </div>
  );
}
