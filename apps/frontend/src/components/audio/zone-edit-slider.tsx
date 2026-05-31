import { cn } from "@/lib/utils.js";

/**
 * Props for a zone-themed range slider.
 */
export interface ZoneEditSliderProps {
  /** Minimum value. */
  min: number;
  /** Maximum value. */
  max: number;
  /** Step increment. */
  step?: number;
  /** Current value. */
  value: number;
  /** Change handler. */
  onChange: (value: number) => void;
  /** Disables interaction. */
  disabled?: boolean;
  /** Accessible label. */
  ariaLabel: string;
  /** Uses zone accent fill when true. */
  accent?: boolean;
  /** Optional class for the input element. */
  className?: string;
}

/**
 * iOS-style range slider with optional zone accent fill.
 * @param props - Slider configuration.
 * @returns Range input element.
 */
export function ZoneEditSlider({
  min,
  max,
  step = 1,
  value,
  onChange,
  disabled = false,
  ariaLabel,
  accent = true,
  className,
}: ZoneEditSliderProps): React.JSX.Element {
  const clamped = Math.max(min, Math.min(max, value));
  const percent = max === min ? 0 : ((clamped - min) / (max - min)) * 100;
  const fillColor = accent ? "var(--zone-accent)" : "var(--muted-foreground)";

  return (
    <input
      type="range"
      min={min}
      max={max}
      step={step}
      value={clamped}
      disabled={disabled}
      aria-label={ariaLabel}
      onChange={(event) => onChange(Number(event.target.value))}
      className={cn(
        "h-1.5 w-full min-w-0 cursor-pointer appearance-none rounded-full outline-none",
        "disabled:cursor-not-allowed disabled:opacity-50",
        "[&::-webkit-slider-runnable-track]:h-1.5 [&::-webkit-slider-runnable-track]:rounded-full",
        "[&::-webkit-slider-thumb]:mt-[-5px] [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:w-4",
        "[&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:rounded-full",
        "[&::-webkit-slider-thumb]:border [&::-webkit-slider-thumb]:border-border [&::-webkit-slider-thumb]:bg-card",
        "[&::-webkit-slider-thumb]:shadow-sm",
        "[&::-moz-range-track]:h-1.5 [&::-moz-range-track]:rounded-full",
        "[&::-moz-range-thumb]:h-4 [&::-moz-range-thumb]:w-4 [&::-moz-range-thumb]:rounded-full",
        "[&::-moz-range-thumb]:border [&::-moz-range-thumb]:border-border [&::-moz-range-thumb]:bg-card",
        className
      )}
      style={{
        background: `linear-gradient(to right, ${fillColor} ${percent}%, var(--muted) ${percent}%)`,
      }}
    />
  );
}
