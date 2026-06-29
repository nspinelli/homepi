import type { ModuleHealth, PlatformHealthEntry } from "@/lib/status-display.js";

import { CapabilityRow } from "./capability-row.js";
import { HealthStatusPill } from "./health-status-pill.js";
import { ServiceHealthDetail } from "./service-health-detail.js";
import { ServiceStatusIcon } from "./service-status-icon.js";

/**
 * Props for a client module health section.
 */
export interface ModuleHealthSectionProps {
  /** Module health rollup. */
  module: ModuleHealth;
}

/**
 * Renders a client-facing module section with icon, status, and capabilities.
 * @param props - Component props.
 * @returns Module section element.
 */
export function ModuleHealthSection({ module }: ModuleHealthSectionProps): React.JSX.Element {
  return (
    <section className="overflow-hidden rounded-lg border border-border bg-card">
      <div className="flex items-center gap-4 border-b border-border px-4 py-4">
        <img
          src={module.icon}
          alt=""
          className="size-20 shrink-0 rounded-lg object-contain sm:size-24"
        />
        <div className="min-w-0 flex-1">
          <h2 className="text-lg font-semibold text-foreground">{module.displayName}</h2>
          {module.userMessage ? (
            <p className="mt-1 text-sm text-muted-foreground">{module.userMessage}</p>
          ) : null}
          {module.stillWorks && module.stillWorks.length > 0 ? (
            <p className="mt-2 text-sm text-foreground">
              Still working: {module.stillWorks.join(", ")}
            </p>
          ) : null}
          <p className="mt-1 font-mono text-xs text-muted-foreground">
            Updated {new Date(module.lastUpdated).toLocaleString()}
          </p>
        </div>
        <HealthStatusPill status={module.status} className="self-center" />
      </div>
      {module.capabilities.map((capability) => (
        <CapabilityRow key={capability.id} capability={capability} />
      ))}
    </section>
  );
}

/**
 * Props for a platform infrastructure section.
 */
export interface PlatformHealthSectionProps {
  /** Platform entries. */
  entries: PlatformHealthEntry[];
}

/**
 * Renders platform/infrastructure health rows.
 * @param props - Component props.
 * @returns Platform section element.
 */
export function PlatformHealthSection({ entries }: PlatformHealthSectionProps): React.JSX.Element {
  return (
    <section className="overflow-hidden rounded-lg border border-border bg-card">
      <div className="border-b border-border px-4 py-4">
        <h2 className="text-lg font-semibold text-foreground">Platform / Infrastructure</h2>
      </div>
      {entries.map((entry) => (
        <div
          key={entry.name}
          className="flex items-start gap-3 border-t border-border/60 px-4 py-3"
        >
          <ServiceStatusIcon serviceId={entry.name} className="mt-0.5" />
          <div className="min-w-0 flex-1">
            <p className="font-medium text-foreground">{entry.name}</p>
            <ServiceHealthDetail
              status={entry.status}
              userMessage={entry.userMessage}
              lastUpdated={entry.lastUpdated}
            />
          </div>
          <HealthStatusPill status={entry.status} className="shrink-0 self-center" />
        </div>
      ))}
    </section>
  );
}
