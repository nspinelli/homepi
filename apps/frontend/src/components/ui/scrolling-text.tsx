import { useLayoutEffect, useRef, useState } from "react";

import { cn } from "@/lib/utils.js";

/**
 * Props for overflow-aware scrolling text.
 */
export interface ScrollingTextProps {
  /** Text to display. */
  text: string;
  /** Optional class names for the text. */
  className?: string;
}

/**
 * Truncates short text and scrolls horizontally when it overflows the container.
 * @param props - Text and styling.
 */
export function ScrollingText({ text, className }: ScrollingTextProps): React.JSX.Element {
  const containerRef = useRef<HTMLDivElement>(null);
  const measureRef = useRef<HTMLSpanElement>(null);
  const [overflows, setOverflows] = useState(false);

  useLayoutEffect(() => {
    const container = containerRef.current;
    const measure = measureRef.current;
    if (!container || !measure) {
      return;
    }

    const updateOverflow = (): void => {
      setOverflows(measure.scrollWidth > container.clientWidth);
    };

    updateOverflow();
    const observer = new ResizeObserver(updateOverflow);
    observer.observe(container);
    return () => observer.disconnect();
  }, [text]);

  const durationSeconds = Math.max(6, text.length * 0.35);

  return (
    <div ref={containerRef} className="relative min-w-0 overflow-hidden">
      <span
        ref={measureRef}
        className={cn("invisible absolute whitespace-nowrap", className)}
        aria-hidden
      >
        {text}
      </span>
      {overflows ? (
        <span
          className={cn("inline-flex whitespace-nowrap animate-scroll-text", className)}
          style={{ animationDuration: `${durationSeconds}s` }}
        >
          <span className="pr-4">{text}</span>
          <span className="pr-4" aria-hidden>
            {text}
          </span>
        </span>
      ) : (
        <span className={cn("block truncate", className)}>{text}</span>
      )}
    </div>
  );
}
