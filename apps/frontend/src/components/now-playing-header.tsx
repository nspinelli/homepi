import { AudioNowPlayingDropdownPanel } from "@/components/audio/audio-now-playing-dropdown-panel.js";
import { AudioBarsIcon } from "@/components/ui/audio-bars-icon.js";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu.js";
import { ScrollingText } from "@/components/ui/scrolling-text.js";
import { useAudioModule } from "@/hooks/audio-module-provider.js";
import { pickDisplayTitle } from "@/lib/audio-metadata-utils.js";
import { cn } from "@/lib/utils.js";

/**
 * Global header now-playing control; opens a dropdown with track details.
 */
export function NowPlayingHeader(): React.JSX.Element | null {
  const { playback } = useAudioModule();
  if (!playback) {
    return null;
  }

  const title = pickDisplayTitle(playback);
  const artist = playback.artist?.trim();
  const ariaLabel = artist ? `Now playing: ${title} by ${artist}` : `Now playing: ${title}`;

  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <button
          type="button"
          aria-label={ariaLabel}
          className={cn(
            "flex max-w-[7.5rem] shrink items-center gap-1.5 sm:max-w-[10rem] md:max-w-[14rem] lg:max-w-[18rem]",
            "px-1 transition-opacity hover:opacity-80",
            "focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring/50"
          )}
        >
          <AudioBarsIcon />
          <span className="min-w-0 flex-1 text-left leading-tight">
            <ScrollingText
              key={`${playback.track ?? ""}:${playback.artist ?? ""}`}
              text={title}
              className="text-[10px] font-medium text-foreground md:text-xs"
            />
            {artist ? (
              <ScrollingText
                key={`${playback.track ?? ""}:${playback.artist ?? ""}:artist`}
                text={artist}
                className="text-[9px] text-muted-foreground md:text-[10px]"
              />
            ) : null}
          </span>
        </button>
      </DropdownMenuTrigger>
      <DropdownMenuContent align="center" className="w-[min(calc(100vw-2rem),26rem)] p-4">
        <AudioNowPlayingDropdownPanel playback={playback} />
      </DropdownMenuContent>
    </DropdownMenu>
  );
}
