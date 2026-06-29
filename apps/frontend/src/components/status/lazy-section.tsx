import type { ReactNode } from "react";

import { Skeleton } from "@/components/ui/skeleton.js";
import { useLazyVisible } from "@/hooks/use-lazy-visible.js";
import { cn } from "@/lib/utils.js";

/** Placeholder module skeleton count shown before REST status data arrives. */
export const INITIAL_MODULE_SKELETON_COUNT = 3;

/**
 * Props for a lazily rendered status section.
 */
export interface LazySectionProps {
  /** Content rendered once the section nears the viewport. */
  children: ReactNode;
  /** Optional custom skeleton; defaults to a generic block. */
  skeleton?: ReactNode;
  /** Placeholder height class when using the default skeleton. */
  skeletonClassName?: string;
}

/**
 * Defers rendering children until the section scrolls near the viewport.
 * @param props - Component props.
 * @returns Lazy section wrapper.
 */
export function LazySection({
  children,
  skeleton,
  skeletonClassName = "h-40",
}: LazySectionProps): React.JSX.Element {
  const { ref, visible } = useLazyVisible();

  return (
    <div ref={ref} className="relative">
      {visible ? (
        <div className="animate-in fade-in-0 duration-300">{children}</div>
      ) : (
        skeleton ?? (
          <Skeleton className={cn("w-full rounded-lg", skeletonClassName)} aria-hidden="true" />
        )
      )}
    </div>
  );
}

/**
 * Skeleton stack shown while module/platform status is still loading.
 * @param props - Component props.
 * @param props.moduleCount - Number of module section placeholders.
 * @returns Loading placeholder elements.
 */
export function StatusSectionsLoadingPlaceholder({
  moduleCount = INITIAL_MODULE_SKELETON_COUNT,
}: {
  moduleCount?: number;
}): React.JSX.Element {
  return (
    <>
      {Array.from({ length: moduleCount }, (_, index) => (
        <ModuleSectionSkeleton key={`module-skeleton-${index}`} />
      ))}
      <PlatformSectionSkeleton />
    </>
  );
}

/**
 * Skeleton placeholder matching a module health section header.
 * @returns Module section skeleton element.
 */
export function ModuleSectionSkeleton(): React.JSX.Element {
  return (
    <div className="overflow-hidden rounded-lg border border-border bg-card">
      <div className="flex items-center gap-4 px-4 py-4">
        <Skeleton className="size-20 shrink-0 rounded-lg sm:size-24" />
        <div className="min-w-0 flex-1 space-y-2">
          <Skeleton className="h-6 w-40" />
          <Skeleton className="h-4 w-64" />
        </div>
        <Skeleton className="h-7 w-20 rounded-full" />
      </div>
      <Skeleton className="mx-4 mb-4 h-10 w-full" />
      <Skeleton className="mx-4 mb-4 h-10 w-full" />
    </div>
  );
}

/**
 * Skeleton placeholder for the platform health section.
 * @returns Platform section skeleton element.
 */
export function PlatformSectionSkeleton(): React.JSX.Element {
  return (
    <div className="overflow-hidden rounded-lg border border-border bg-card">
      <div className="border-b border-border px-4 py-4">
        <Skeleton className="h-6 w-56" />
      </div>
      <Skeleton className="mx-4 my-3 h-12 w-full" />
      <Skeleton className="mx-4 my-3 h-12 w-full" />
      <Skeleton className="mx-4 my-3 h-12 w-full" />
    </div>
  );
}
