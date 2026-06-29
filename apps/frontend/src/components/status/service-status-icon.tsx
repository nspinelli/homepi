import { resolveServiceStatusIcon } from "@/lib/service-status-icons.js";
import { cn } from "@/lib/utils.js";

/**
 * Props for a service or capability status icon.
 */
export interface ServiceStatusIconProps {
  /** Capability id or platform service name. */
  serviceId: string;
  /** Optional additional class names. */
  className?: string;
}

/**
 * Renders a Lucide icon describing a capability or platform service.
 * @param props - Component props.
 * @returns Service icon element.
 */
export function ServiceStatusIcon({ serviceId, className }: ServiceStatusIconProps): React.JSX.Element {
  const Icon = resolveServiceStatusIcon(serviceId);

  return (
    <Icon
      className={cn("size-4 shrink-0 text-muted-foreground", className)}
      aria-hidden="true"
    />
  );
}
