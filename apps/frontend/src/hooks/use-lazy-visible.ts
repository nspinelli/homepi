import { useEffect, useRef, useState, type RefObject } from "react";

/**
 * Options for lazy visibility detection.
 */
export interface UseLazyVisibleOptions {
  /** IntersectionObserver root margin for early loading. */
  rootMargin?: string;
  /** Intersection threshold. */
  threshold?: number;
}

/**
 * Tracks whether an element is near or inside the viewport for lazy rendering.
 * @param options - Observer options.
 * @returns Ref to attach and visibility flag.
 */
export function useLazyVisible<T extends HTMLElement = HTMLDivElement>(
  options: UseLazyVisibleOptions = {}
): {
  ref: RefObject<T | null>;
  visible: boolean;
} {
  const ref = useRef<T | null>(null);
  const [visible, setVisible] = useState(false);

  useEffect(() => {
    const element = ref.current;
    if (!element || visible) {
      return;
    }

    const marginPx = parseRootMarginPx(options.rootMargin ?? "200px 0px");
    const rect = element.getBoundingClientRect();
    if (rect.top < window.innerHeight + marginPx && rect.bottom > -marginPx) {
      setVisible(true);
      return;
    }

    const observer = new IntersectionObserver(
      (entries) => {
        if (entries.some((entry) => entry.isIntersecting)) {
          setVisible(true);
          observer.disconnect();
        }
      },
      {
        rootMargin: options.rootMargin ?? "200px 0px",
        threshold: options.threshold ?? 0,
      }
    );

    observer.observe(element);
    return () => {
      observer.disconnect();
    };
  }, [options.rootMargin, options.threshold, visible]);

  return { ref, visible };
}

/**
 * Parses the vertical root margin from an IntersectionObserver rootMargin string.
 * @param rootMargin - CSS margin shorthand (e.g. `200px 0px`).
 * @returns Top/bottom margin in pixels.
 */
function parseRootMarginPx(rootMargin: string): number {
  const top = rootMargin.trim().split(/\s+/)[0] ?? "0px";
  const value = Number.parseFloat(top);
  return Number.isFinite(value) ? value : 0;
}
