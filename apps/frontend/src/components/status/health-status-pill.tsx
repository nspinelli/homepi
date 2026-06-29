import { Badge } from "@/components/ui/badge.js";
import { mapHealthToVisual, type ServiceVisualStatus } from "@/lib/status-display.js";
import { cn } from "@/lib/utils.js";

const PILL_STYLES: Record<ServiceVisualStatus, string> = {
  online: "border-emerald-500/30 bg-emerald-500/10 text-emerald-400",
  warning: "border-amber-500/30 bg-amber-500/10 text-amber-400",
  offline: "border-red-500/30 bg-red-500/10 text-red-400",
};

/**
 * Props for the health status pill badge.
 */
export interface HealthStatusPillProps {
  /** Raw health status string from the API. */
  status: string;
  /** Optional additional class names. */
  className?: string;
}

/**
 * Formats a health status string for display in a pill.
 * @param status - Raw health status.
 * @returns Human-readable label.
 */
function formatHealthStatusLabel(status: string): string {
  if (status === "healthy" || status === "pass") {
    return "Healthy";
  }
  if (status === "degraded" || status === "warn") {
    return "Degraded";
  }
  if (status === "offline" || status === "fail" || status === "failed") {
    return "Offline";
  }
  if (status === "starting") {
    return "Starting";
  }
  if (status === "stopping") {
    return "Stopping";
  }
  if (status === "unknown") {
    return "Unknown";
  }
  return status.charAt(0).toUpperCase() + status.slice(1);
}

/**
 * Renders a colored pill for module or service health status.
 * @param props - Component props.
 * @returns Status pill element.
 */
export function HealthStatusPill({ status, className }: HealthStatusPillProps): React.JSX.Element {
  const visual = mapHealthToVisual(status);

  return (
    <Badge
      variant="outline"
      className={cn("rounded-full px-3 py-1 text-xs font-medium capitalize", PILL_STYLES[visual], className)}
    >
      {formatHealthStatusLabel(status)}
    </Badge>
  );
}
