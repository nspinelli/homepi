import { Card, CardContent } from "@/components/ui/card.js";

/**
 * Skeleton placeholder for the contact sensors dashboard card during REST bootstrap.
 */
export function SensorsCardSkeleton(): React.JSX.Element {
  return (
    <Card>
      <CardContent className="px-6 py-3">
        <div className="flex items-center gap-4">
          <div className="h-28 w-28 shrink-0 animate-pulse rounded-lg bg-secondary" />
          <div className="space-y-3">
            <div className="h-6 w-40 animate-pulse rounded bg-secondary" />
            <div className="h-5 w-24 animate-pulse rounded-full bg-secondary" />
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
