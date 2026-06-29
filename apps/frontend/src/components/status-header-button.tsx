import { Activity, HeartPulse, LayoutDashboard, Thermometer, Wifi, type LucideIcon } from "lucide-react";
import { Link } from "react-router-dom";

import { Button } from "@/components/ui/button.js";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu.js";
import { useSystemDashboard } from "@/hooks/system-dashboard-provider.js";
import {
  formatCpuTemp,
  hasDashboardWarnings,
  mapCpuTempStatus,
  mapHealthToVisual,
  statusDotClass,
  statusIconClass,
  summarizeHeaderIconStatus,
  summarizeOverallHealth,
} from "@/lib/status-display.js";
import { cn } from "@/lib/utils.js";

/**
 * Props for a labeled status row in the header dropdown.
 */
interface StatusDropdownRowProps {
  /** Row icon component. */
  icon: LucideIcon;
  /** Left-hand label. */
  label: string;
  /** Visual status for the indicator dot. */
  status: "online" | "warning" | "offline";
  /** Right-hand value text. */
  value: string;
}

/**
 * Single status row with icon + label on the left and aligned dot + value on the right.
 * @param props - Row display options.
 */
function StatusDropdownRow({
  icon: Icon,
  label,
  status,
  value,
}: StatusDropdownRowProps): React.JSX.Element {
  return (
    <div className="flex items-center py-0.5">
      <div className="flex min-w-0 flex-1 items-center gap-2 pr-8 text-sm text-muted-foreground">
        <Icon className="size-4 shrink-0" aria-hidden />
        <span className="truncate">{label}</span>
      </div>
      <div className="flex shrink-0 items-center gap-1.5">
        <span className="flex w-2 shrink-0 justify-center">
          <span className={`size-2 rounded-full ${statusDotClass(status)}`} aria-hidden />
        </span>
        <span className="min-w-[4.5rem] truncate text-right text-sm text-foreground">{value}</span>
      </div>
    </div>
  );
}

/**
 * Header status icon with a compact system health dropdown.
 */
export function StatusHeaderButton(): React.JSX.Element {
  const { state } = useSystemDashboard();
  const host = state.coreStatus?.host;
  const overallLabel = summarizeOverallHealth(state.health, state.coreStatus);
  const hasWarnings = hasDashboardWarnings(state);
  const tempStatus = mapCpuTempStatus(host?.cpuTempC ?? state.hostMetrics?.cpuTempC);
  const loading = state.loading && !state.coreStatus;
  const iconStatus = loading ? null : summarizeHeaderIconStatus(overallLabel, hasWarnings);
  const transportStatus =
    state.sseState === "error" || state.wsState === "error" ? "offline" : "online";

  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <Button
          type="button"
          variant="ghost"
          size="icon"
          className="group relative shrink-0 px-2"
          aria-label="System status"
        >
          <Activity
            className={cn(
              "size-4 transition-colors",
              iconStatus
                ? cn(statusIconClass(iconStatus), "group-hover:text-accent-foreground")
                : "text-muted-foreground group-hover:text-accent-foreground"
            )}
            aria-hidden
          />
          {hasWarnings ? (
            <span className="absolute top-1 right-1 size-2 rounded-full bg-amber-500" aria-hidden />
          ) : null}
        </Button>
      </DropdownMenuTrigger>
      <DropdownMenuContent align="end" className="w-max min-w-[14rem] max-w-none p-2">
        {state.error ? (
          <p className="mb-2 px-2 text-xs text-destructive">{state.error}</p>
        ) : null}
        {state.transportError ? (
          <p className="mb-2 px-2 text-xs text-amber-500">{state.transportError}</p>
        ) : null}
        <div className="space-y-1 px-2 py-1">
          <StatusDropdownRow
            icon={Thermometer}
            label="Temperature"
            status={tempStatus}
            value={loading ? "…" : formatCpuTemp(host?.cpuTempC ?? state.hostMetrics?.cpuTempC)}
          />
          <StatusDropdownRow
            icon={HeartPulse}
            label="Health"
            status={mapHealthToVisual(overallLabel)}
            value={loading ? "…" : overallLabel}
          />
          <StatusDropdownRow
            icon={Wifi}
            label="Live updates"
            status={transportStatus}
            value={state.sseState === "connected" && state.wsState === "connected" ? "connected" : "degraded"}
          />
        </div>
        <div className="my-1 h-px bg-border" />
        <DropdownMenuItem asChild className="whitespace-nowrap">
          <Link to="/status" className="w-full cursor-pointer">
            <LayoutDashboard aria-hidden />
            View system status
          </Link>
        </DropdownMenuItem>
      </DropdownMenuContent>
    </DropdownMenu>
  );
}
