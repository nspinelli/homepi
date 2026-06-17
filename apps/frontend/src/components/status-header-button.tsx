import { Activity, HeartPulse, LayoutDashboard, Thermometer, type LucideIcon } from "lucide-react";
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
  mapCpuTempStatus,
  statusDotClass,
  statusIconClass,
  summarizeHeaderIconStatus,
  summarizeSystemOverall,
  type ServiceVisualStatus,
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
  status: ServiceVisualStatus;
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
          <span
            className={`size-2 rounded-full ${statusDotClass(status)}`}
            aria-hidden
          />
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
  const overall = summarizeSystemOverall(state.systemStatus, state.health?.status);
  const tempStatus = mapCpuTempStatus(state.systemStatus?.cpuTempC);
  const loading = state.loading && !state.systemStatus;
  const iconStatus = loading
    ? null
    : summarizeHeaderIconStatus(
        state.systemStatus,
        state.health?.status,
        state.systemStatus?.cpuTempC
      );

  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <Button
          type="button"
          variant="ghost"
          size="icon"
          className="group shrink-0 px-2"
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
        </Button>
      </DropdownMenuTrigger>
      <DropdownMenuContent align="end" className="w-max min-w-[14rem] max-w-none p-2">
        <div className="space-y-1 px-2 py-1">
          <StatusDropdownRow
            icon={Thermometer}
            label="Temperature"
            status={tempStatus}
            value={loading ? "…" : formatCpuTemp(state.systemStatus?.cpuTempC)}
          />
          <StatusDropdownRow
            icon={HeartPulse}
            label="Health"
            status={overall.status}
            value={loading ? "…" : overall.label}
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
