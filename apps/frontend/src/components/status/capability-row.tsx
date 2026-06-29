import type { CapabilityHealth } from "@/lib/status-display.js";

import { HealthStatusPill } from "./health-status-pill.js";
import { ServiceHealthDetail } from "./service-health-detail.js";
import { ServiceStatusIcon } from "./service-status-icon.js";

/**
 * Props for a single capability health row.
 */
export interface CapabilityRowProps {
  /** Capability health data. */
  capability: CapabilityHealth;
}

/**
 * Renders one capability row under a module section.
 * @param props - Component props.
 * @returns Capability row element.
 */
export function CapabilityRow({ capability }: CapabilityRowProps): React.JSX.Element {
  return (
    <div className="flex items-start gap-3 border-t border-border/60 px-4 py-3">
      <ServiceStatusIcon serviceId={capability.id} className="mt-0.5" />
      <div className="min-w-0 flex-1">
        <p className="font-medium text-foreground">{capability.displayName}</p>
        <ServiceHealthDetail
          status={capability.status}
          userMessage={capability.userMessage}
          process={capability.process}
          readiness={capability.readiness}
          domain={capability.domain}
          lastUpdated={capability.lastUpdated}
        />
      </div>
      <HealthStatusPill status={capability.status} className="shrink-0 self-center" />
    </div>
  );
}
