import { useCallback, useRef } from "react";

import { cn } from "@/lib/utils.js";

/** Default diameter of the circular volume control in pixels. */
const DEFAULT_SIZE = 44;

/** Ring stroke width in pixels. */
const STROKE_WIDTH = 3;

/**
 * Props for the circular zone volume control.
 */
export interface ZoneVolumeKnobProps {
  /** Current volume 0–100. */
  value: number;
  /** Whether the zone accent ring should be active. */
  isActive: boolean;
  /** Disables interaction when the zone cannot be adjusted. */
  disabled?: boolean;
  /** Called when the user changes volume. */
  onChange: (value: number) => void;
  /** Accessible label for the control. */
  ariaLabel?: string;
  /** Outer diameter in pixels. */
  size?: number;
}

/**
 * Maps a pointer position to a 0–100 volume value on a circular control.
 * @param clientX - Pointer X coordinate.
 * @param clientY - Pointer Y coordinate.
 * @param rect - Bounding box of the control.
 * @returns Volume 0–100.
 */
function pointerToVolume(clientX: number, clientY: number, rect: DOMRect): number {
  const centerX = rect.left + rect.width / 2;
  const centerY = rect.top + rect.height / 2;
  const angle = Math.atan2(clientY - centerY, clientX - centerX);
  const normalized = (angle + Math.PI / 2 + 2 * Math.PI) % (2 * Math.PI);
  return Math.max(0, Math.min(100, Math.round((normalized / (2 * Math.PI)) * 100)));
}

/**
 * Circular volume knob for compact zone quick actions.
 * @param props - Knob state and handlers.
 */
export function ZoneVolumeKnob({
  value,
  isActive,
  disabled = false,
  onChange,
  ariaLabel = "Zone volume",
  size = DEFAULT_SIZE,
}: ZoneVolumeKnobProps): React.JSX.Element {
  const knobRef = useRef<HTMLDivElement>(null);
  const radius = (size - STROKE_WIDTH) / 2;
  const circumference = 2 * Math.PI * radius;
  const clamped = Math.max(0, Math.min(100, value));
  const dashOffset = circumference * (1 - clamped / 100);
  const ringColor = isActive ? "var(--zone-accent)" : "var(--muted-foreground)";

  const updateFromPointer = useCallback(
    (clientX: number, clientY: number) => {
      const element = knobRef.current;
      if (!element || disabled) {
        return;
      }
      onChange(pointerToVolume(clientX, clientY, element.getBoundingClientRect()));
    },
    [disabled, onChange]
  );

  const handlePointerDown = (event: React.PointerEvent<HTMLDivElement>): void => {
    if (disabled) {
      return;
    }
    event.preventDefault();
    event.currentTarget.setPointerCapture(event.pointerId);
    updateFromPointer(event.clientX, event.clientY);
  };

  const handlePointerMove = (event: React.PointerEvent<HTMLDivElement>): void => {
    if (disabled || !event.currentTarget.hasPointerCapture(event.pointerId)) {
      return;
    }
    updateFromPointer(event.clientX, event.clientY);
  };

  const handlePointerUp = (event: React.PointerEvent<HTMLDivElement>): void => {
    if (event.currentTarget.hasPointerCapture(event.pointerId)) {
      event.currentTarget.releasePointerCapture(event.pointerId);
    }
  };

  const handleKeyDown = (event: React.KeyboardEvent<HTMLDivElement>): void => {
    if (disabled) {
      return;
    }
    if (event.key === "ArrowUp" || event.key === "ArrowRight") {
      event.preventDefault();
      onChange(Math.min(100, clamped + 1));
    } else if (event.key === "ArrowDown" || event.key === "ArrowLeft") {
      event.preventDefault();
      onChange(Math.max(0, clamped - 1));
    }
  };

  return (
    <div
      ref={knobRef}
      role="slider"
      tabIndex={disabled ? -1 : 0}
      aria-label={ariaLabel}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-valuenow={clamped}
      aria-disabled={disabled}
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={handlePointerUp}
      onKeyDown={handleKeyDown}
      onClick={(event) => event.stopPropagation()}
      className={cn(
        "relative shrink-0 touch-none select-none rounded-full outline-none",
        disabled ? "cursor-not-allowed opacity-50" : "cursor-pointer",
        "focus-visible:ring-2 focus-visible:ring-ring/50"
      )}
      style={{ width: size, height: size }}
    >
      <svg
        width={size}
        height={size}
        viewBox={`0 0 ${size} ${size}`}
        className="block -rotate-90"
        aria-hidden
      >
        <circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          fill="none"
          stroke="var(--muted)"
          strokeWidth={STROKE_WIDTH}
        />
        <circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          fill="none"
          stroke={ringColor}
          strokeWidth={STROKE_WIDTH}
          strokeLinecap="round"
          strokeDasharray={circumference}
          strokeDashoffset={dashOffset}
          className="transition-[stroke-dashoffset] duration-150"
        />
      </svg>
      <span className="pointer-events-none absolute inset-0 flex items-center justify-center text-[10px] font-medium tabular-nums text-muted-foreground">
        {clamped}
      </span>
    </div>
  );
}
