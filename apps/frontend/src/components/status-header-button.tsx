import { Activity } from "lucide-react";
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
  summarizeSystemOverall,
} from "@/lib/status-display.js";

/**
 * Header status icon with a compact system health dropdown.
 */
export function StatusHeaderButton(): React.JSX.Element {
  const { state } = useSystemDashboard();
  const overall = summarizeSystemOverall(state.systemStatus, state.health?.status);
  const tempStatus = mapCpuTempStatus(state.systemStatus?.cpuTempC);

  return (
    <DropdownMenu>
      <DropdownMenuTrigger asChild>
        <Button
          type="button"
          variant="ghost"
          size="icon"
          className="shrink-0 px-2"
          aria-label="System status"
        >
          <Activity className="size-4" aria-hidden />
        </Button>
      </DropdownMenuTrigger>
      <DropdownMenuContent align="end" className="w-max max-w-none p-2">
        <div className="space-y-1 whitespace-nowrap px-2 py-1">
          <div className="flex items-center gap-2 text-sm">
            <span className="text-muted-foreground">CPU temperature</span>
            <span className="flex items-center gap-2 font-mono text-foreground">
              <span className={`size-2 shrink-0 rounded-full ${statusDotClass(tempStatus)}`} aria-hidden />
              {state.loading && !state.systemStatus
                ? "…"
                : formatCpuTemp(state.systemStatus?.cpuTempC)}
            </span>
          </div>
          <div className="flex items-center gap-2 text-sm">
            <span className="text-muted-foreground">Overall status</span>
            <span className="flex items-center gap-2 capitalize text-foreground">
              <span className={`size-2 shrink-0 rounded-full ${statusDotClass(overall.status)}`} aria-hidden />
              {state.loading && !state.systemStatus ? "…" : overall.label}
            </span>
          </div>
        </div>
        <div className="my-1 h-px bg-border" />
        <DropdownMenuItem asChild className="whitespace-nowrap">
          <Link to="/status" className="w-full cursor-pointer">
            View system status
          </Link>
        </DropdownMenuItem>
      </DropdownMenuContent>
    </DropdownMenu>
  );
}
