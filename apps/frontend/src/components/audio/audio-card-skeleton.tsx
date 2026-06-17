import { Card, CardContent } from "@/components/ui/card.js";
import { Skeleton } from "@/components/ui/skeleton.js";

/**
 * Placeholder for the home dashboard audio module card during initial load.
 */
export function AudioCardSkeleton(): React.JSX.Element {
  return (
    <Card className="border border-border shadow-sm" aria-busy="true" aria-label="Loading audio">
      <CardContent className="px-6 py-3">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-4">
            <Skeleton className="h-28 w-28 shrink-0 rounded-lg" />
            <div className="space-y-2">
              <Skeleton className="h-5 w-28" />
              <Skeleton className="h-5 w-24 rounded-full" />
            </div>
          </div>
          <Skeleton className="h-5 w-5 rounded-full" />
        </div>
      </CardContent>
    </Card>
  );
}
