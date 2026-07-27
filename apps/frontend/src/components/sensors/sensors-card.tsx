import { ChevronRight } from "lucide-react";
import { Link } from "react-router-dom";

import { Card, CardContent } from "@/components/ui/card.js";
import {
  sensorsConnectionLabel,
  type SensorsConnectionLevel,
} from "@/lib/derive-sensors-connection-level.js";
import { cn } from "@/lib/utils.js";

/** Contact sensors module artwork used on the home dashboard card. */
const SENSORS_MODULE_ICON = "/sensors-module.png?v=3";

/**
 * Props for the home dashboard contact sensors module card.
 */
export interface SensorsCardProps {
  /** Display name for the module. */
  name: string;
  /** Aggregate connection health for the status pill. */
  connectionLevel: SensorsConnectionLevel;
  /** Optional open sensor count for subtitle. */
  openCount?: number;
}

const PILL_DOT_CLASS: Record<SensorsConnectionLevel, string> = {
  healthy: "bg-success",
  degraded: "bg-warning",
  offline: "bg-destructive",
};

const PILL_BG_CLASS: Record<SensorsConnectionLevel, string> = {
  healthy: "bg-success/15",
  degraded: "bg-warning/15",
  offline: "bg-destructive/15",
};

/**
 * Dashboard card for contact sensors; links to /contact-sensors.
 */
export function SensorsCard({
  name,
  connectionLevel,
  openCount,
}: SensorsCardProps): React.JSX.Element {
  const statusLabel = sensorsConnectionLabel(connectionLevel);

  return (
    <Link to="/contact-sensors" className="block">
      <Card className="group cursor-pointer transition-colors hover:bg-secondary/50">
        <CardContent className="px-6 py-3">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-4">
              <img
                src={SENSORS_MODULE_ICON}
                alt=""
                width={112}
                height={112}
                className="h-28 w-28 shrink-0 object-contain"
              />
              <div>
                <h3 className="text-lg font-semibold text-foreground">{name}</h3>
                <span
                  className={cn(
                    "mt-1.5 inline-flex items-center gap-1.5 rounded-full px-2.5 py-0.5 text-xs font-medium text-foreground",
                    PILL_BG_CLASS[connectionLevel]
                  )}
                >
                  <span
                    className={cn(
                      "h-1.5 w-1.5 shrink-0 rounded-full",
                      PILL_DOT_CLASS[connectionLevel]
                    )}
                    aria-hidden
                  />
                  {statusLabel}
                </span>
                {typeof openCount === "number" && connectionLevel !== "offline" ? (
                  <p className="mt-2 text-sm text-muted-foreground">
                    {openCount} open · live
                  </p>
                ) : null}
              </div>
            </div>
            <ChevronRight className="h-5 w-5 text-muted-foreground transition-transform group-hover:translate-x-1" />
          </div>
        </CardContent>
      </Card>
    </Link>
  );
}
