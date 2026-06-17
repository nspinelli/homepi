import { Card, CardContent } from "@/components/ui/card.js";
import { Skeleton } from "@/components/ui/skeleton.js";

/**
 * Placeholder for the home dashboard audio module card during initial load.
 */
export function AudioCardSkeleton(): React.JSX.Element {
  return (
    <Card className="border border-border shadow-sm" aria-busy="true" aria-label="Loading audio">
      <CardContent className="p-6">
        <div className="flex items-start justify-between">
          <div className="flex items-center gap-4">
            <Skeleton className="h-16 w-16 shrink-0 rounded-lg" />
            <div className="space-y-2">
              <Skeleton className="h-5 w-28" />
              <Skeleton className="h-4 w-24" />
            </div>
          </div>
          <Skeleton className="h-5 w-5 rounded-full" />
        </div>
        <div className="mt-6 border-t border-border pt-4">
          <Skeleton className="mb-3 h-3 w-24" />
          <div className="flex flex-wrap gap-2">
            <Skeleton className="h-6 w-20 rounded-full" />
            <Skeleton className="h-6 w-24 rounded-full" />
            <Skeleton className="h-6 w-16 rounded-full" />
            <Skeleton className="h-6 w-20 rounded-full" />
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
