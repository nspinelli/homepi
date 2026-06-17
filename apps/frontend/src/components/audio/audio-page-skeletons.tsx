import { Card, CardContent } from "@/components/ui/card.js";
import { Skeleton } from "@/components/ui/skeleton.js";

/**
 * Props for the zone card grid skeleton.
 */
export interface AudioZonesGridSkeletonProps {
  /** Number of placeholder cards to render. */
  count?: number;
}

/**
 * Placeholder matching a single zone card layout.
 */
export function ZoneCardSkeleton(): React.JSX.Element {
  return (
    <Card className="min-w-0 border border-border shadow-sm">
      <CardContent className="p-4">
        <div className="flex items-start justify-between gap-3">
          <div className="flex min-w-0 flex-1 items-center gap-3">
            <Skeleton className="h-11 w-11 shrink-0 rounded-full" />
            <div className="min-w-0 flex-1 space-y-2">
              <Skeleton className="h-4 w-3/5" />
              <Skeleton className="h-3 w-1/4" />
            </div>
          </div>
          <Skeleton className="h-8 w-8 shrink-0 rounded-full" />
        </div>
        <div className="mt-4 flex items-center gap-3">
          <Skeleton className="h-4 w-4 shrink-0 rounded-full" />
          <Skeleton className="h-1.5 flex-1 rounded-full" />
          <Skeleton className="h-4 w-9 shrink-0" />
        </div>
        <div className="mt-4 flex items-center justify-between border-t border-border pt-3">
          <Skeleton className="h-4 w-2/5" />
          <Skeleton className="h-4 w-4 rounded-full" />
        </div>
      </CardContent>
    </Card>
  );
}

/**
 * Grid of zone card placeholders for the initial audio page load.
 * @param props - Skeleton count.
 */
export function AudioZonesGridSkeleton({
  count = 8,
}: AudioZonesGridSkeletonProps): React.JSX.Element {
  return (
    <div
      className="grid min-w-0 grid-cols-1 gap-6 md:grid-cols-2"
      aria-busy="true"
      aria-label="Loading audio zones"
    >
      {Array.from({ length: count }, (_, index) => (
        <ZoneCardSkeleton key={index} />
      ))}
    </div>
  );
}

/**
 * Props for a generic list section skeleton.
 */
export interface AudioListSectionSkeletonProps {
  /** Section heading width class. */
  titleWidth?: string;
  /** Number of list rows. */
  rows?: number;
}

/**
 * Placeholder for sources, groups, or settings list panels.
 * @param props - Layout options.
 */
export function AudioListSectionSkeleton({
  titleWidth = "w-32",
  rows = 4,
}: AudioListSectionSkeletonProps): React.JSX.Element {
  return (
    <div
      className="rounded-lg border border-border bg-card p-6"
      aria-busy="true"
      aria-label="Loading section"
    >
      <Skeleton className={["h-6", titleWidth].join(" ")} />
      <Skeleton className="mt-2 h-4 w-2/5" />
      <ul className="mt-4 grid gap-2">
        {Array.from({ length: rows }, (_, index) => (
          <li key={index}>
            <Skeleton className="h-10 w-full rounded-md" />
          </li>
        ))}
      </ul>
    </div>
  );
}
