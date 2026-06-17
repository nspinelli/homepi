import { cn } from "@/lib/utils.js";

/**
 * Props for the animated audio bars icon.
 */
export interface AudioBarsIconProps {
  /** Optional class names for the container. */
  className?: string;
}

/**
 * Three vertical bars that animate like an equalizer while audio plays.
 * @param props - Styling options.
 */
export function AudioBarsIcon({ className }: AudioBarsIconProps): React.JSX.Element {
  return (
    <span
      className={cn("flex h-3.5 w-3.5 shrink-0 items-end justify-center gap-[2px]", className)}
      aria-hidden
    >
      <span className="h-3 w-[2px] origin-bottom rounded-full bg-emerald-500 animate-audio-bar-1" />
      <span className="h-3 w-[2px] origin-bottom rounded-full bg-emerald-500 animate-audio-bar-2" />
      <span className="h-3 w-[2px] origin-bottom rounded-full bg-emerald-500 animate-audio-bar-3" />
    </span>
  );
}
