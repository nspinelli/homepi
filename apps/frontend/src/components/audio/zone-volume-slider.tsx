import { cn } from "@/lib/utils.js";

/**
 * Props for the zone volume range input.
 */
export interface ZoneVolumeSliderProps {
  /** Current volume 0–100. */
  value: number;
  /** Whether the zone accent fill should be active. */
  isActive: boolean;
  /** Disables interaction when the zone cannot be adjusted. */
  disabled?: boolean;
  /** Called when the user changes volume. */
  onChange: (value: number) => void;
  /** Optional aria label override. */
  ariaLabel?: string;
}

/**
 * Horizontal volume slider styled for zone cards and now-playing quick actions.
 * @param props - Slider state and handlers.
 */
export function ZoneVolumeSlider({
  value,
  isActive,
  disabled = false,
  onChange,
  ariaLabel = "Zone volume",
}: ZoneVolumeSliderProps): React.JSX.Element {
  const clamped = Math.max(0, Math.min(100, value));
  const fillPercent = `${clamped}%`;

  return (
    <input
      type="range"
      min={0}
      max={100}
      step={1}
      value={clamped}
      disabled={disabled}
      aria-label={ariaLabel}
      onClick={(event) => event.stopPropagation()}
      onChange={(event) => onChange(Number(event.target.value))}
      className={cn(
        "h-1.5 w-full cursor-pointer appearance-none rounded-full outline-none",
        "disabled:cursor-not-allowed disabled:opacity-50",
        "[&::-webkit-slider-runnable-track]:h-1.5 [&::-webkit-slider-runnable-track]:rounded-full",
        "[&::-webkit-slider-thumb]:mt-[-5px] [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:w-4",
        "[&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:rounded-full",
        "[&::-webkit-slider-thumb]:border [&::-webkit-slider-thumb]:border-border [&::-webkit-slider-thumb]:bg-card",
        "[&::-webkit-slider-thumb]:shadow-sm",
        "[&::-moz-range-track]:h-1.5 [&::-moz-range-track]:rounded-full",
        "[&::-moz-range-thumb]:h-4 [&::-moz-range-thumb]:w-4 [&::-moz-range-thumb]:rounded-full",
        "[&::-moz-range-thumb]:border [&::-moz-range-thumb]:border-border [&::-moz-range-thumb]:bg-card"
      )}
      style={{
        background: `linear-gradient(to right, ${
          isActive ? "var(--zone-accent)" : "var(--muted-foreground)"
        } ${fillPercent}, var(--muted) ${fillPercent})`,
      }}
    />
  );
}
