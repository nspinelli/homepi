import * as React from "react";

import { cn } from "@/lib/utils.js";

/**
 * Props for the animated placeholder skeleton.
 */
export interface SkeletonProps extends React.ComponentProps<"div"> {}

/**
 * Pulsing placeholder block for lazy-loaded content.
 * @param props - Standard div props plus optional className.
 */
export function Skeleton({ className, ...props }: SkeletonProps): React.JSX.Element {
  return (
    <div
      data-slot="skeleton"
      className={cn("animate-pulse rounded-md bg-muted", className)}
      {...props}
    />
  );
}
