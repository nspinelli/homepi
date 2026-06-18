import type { ReactNode } from "react";

import { cn } from "@/lib/utils.js";

/**
 * Props for a module detail page header with icon, title, and subtitle.
 */
export interface ModulePageHeaderProps {
  /** Module icon image URL. */
  iconSrc: string;
  /** Accessible label for the icon; use empty string when decorative. */
  iconAlt?: string;
  /** Primary page or section title. */
  title: string;
  /** Secondary line (counts, status, loading text). */
  subtitle?: string;
  /** Optional trailing controls (filters, refresh, etc.). */
  actions?: ReactNode;
  /** Optional class names for the outer row. */
  className?: string;
}

/**
 * Shared module page header: icon and text vertically centered in one row.
 * @param props - Header content and optional actions.
 */
export function ModulePageHeader({
  iconSrc,
  iconAlt = "",
  title,
  subtitle,
  actions,
  className,
}: ModulePageHeaderProps): React.JSX.Element {
  return (
    <div className={cn("mb-6 flex items-center justify-between gap-4", className)}>
      <div className="flex min-w-0 items-center gap-4">
        <img
          src={iconSrc}
          alt={iconAlt}
          width={80}
          height={80}
          className="h-20 w-20 shrink-0 object-contain"
        />
        <div className="min-w-0">
          <h1 className="text-lg font-semibold text-foreground">{title}</h1>
          {subtitle ? (
            <p className="mt-0.5 text-sm text-muted-foreground">{subtitle}</p>
          ) : null}
        </div>
      </div>
      {actions ? <div className="shrink-0 self-center">{actions}</div> : null}
    </div>
  );
}
