import { useMemo, useState } from "react";
import { Link } from "react-router-dom";
import { ArrowLeft, ChevronDown, Search } from "lucide-react";

import { Badge } from "@/components/ui/badge.js";
import { Button } from "@/components/ui/button.js";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu.js";
import { Input } from "@/components/ui/input.js";
import { ScrollArea } from "@/components/ui/scroll-area.js";
import {
  buildLogEntries,
  buildServiceCards,
  formatLogTime,
  formatTimestamp,
  formatUptime,
  type LogLevel,
  type ServiceVisualStatus,
} from "@/lib/status-display.js";
import { useSystemDashboard } from "@/hooks/use-system-dashboard.js";

const STATUS_COLORS: Record<ServiceVisualStatus, string> = {
  online: "bg-emerald-500",
  warning: "bg-amber-500",
  offline: "bg-red-500",
};

const LEVEL_COLORS: Record<LogLevel, string> = {
  info: "bg-sky-500/10 text-sky-400 border-sky-500/20",
  warning: "bg-amber-500/10 text-amber-400 border-amber-500/20",
  error: "bg-red-500/10 text-red-400 border-red-500/20",
  debug: "bg-zinc-500/10 text-zinc-400 border-zinc-500/20",
};

/**
 * System status route populated from live REST, SSE, and WebSocket data.
 */
export function StatusPage(): React.JSX.Element {
  const { state, refresh } = useSystemDashboard();
  const [selectedLevel, setSelectedLevel] = useState<LogLevel | "all">("all");
  const [selectedService, setSelectedService] = useState<string>("all");
  const [searchQuery, setSearchQuery] = useState("");

  const services = useMemo(
    () =>
      buildServiceCards(state.systemStatus, {
        sse: state.sseState,
        ws: state.wsState,
      }),
    [state.systemStatus, state.sseState, state.wsState]
  );

  const logs = useMemo(() => buildLogEntries(state.recentEvents), [state.recentEvents]);

  const filteredLogs = logs.filter((log) => {
    const matchesLevel = selectedLevel === "all" || log.level === selectedLevel;
    const matchesService = selectedService === "all" || log.service === selectedService;
    const matchesSearch =
      searchQuery === "" ||
      log.message.toLowerCase().includes(searchQuery.toLowerCase()) ||
      log.service.toLowerCase().includes(searchQuery.toLowerCase());
    return matchesLevel && matchesService && matchesSearch;
  });

  return (
    <main className="mx-auto max-w-4xl px-4 py-6">
      <div className="mb-6">
        <Button variant="ghost" size="sm" className="mb-4 gap-2 text-muted-foreground" asChild>
          <Link to="/">
            <ArrowLeft className="size-4" />
            Back
          </Link>
        </Button>
        <div className="flex flex-wrap items-start justify-between gap-4">
          <div>
            <h1 className="text-2xl font-semibold text-foreground">System Status</h1>
            <p className="mt-1 text-sm text-muted-foreground">
              Monitor platform services and live event stream
            </p>
          </div>
          <Button
            type="button"
            variant="secondary"
            size="sm"
            onClick={() => {
              void refresh();
            }}
          >
            Refresh
          </Button>
        </div>
      </div>

      {state.error ? (
        <p className="mb-4 rounded-lg border border-destructive/30 bg-destructive/10 px-4 py-3 text-sm text-destructive">
          {state.error}
        </p>
      ) : null}
      {state.loading ? (
        <p className="mb-4 text-sm text-muted-foreground">Loading platform status…</p>
      ) : null}

      <div className="mb-8 grid grid-cols-1 gap-4 sm:grid-cols-2">
        <div className="rounded-lg border border-border bg-card p-4">
          <p className="text-sm text-muted-foreground">Platform uptime</p>
          <p className="mt-1 font-mono text-lg text-foreground">
            {formatUptime(state.systemStatus?.uptimeMs)}
          </p>
        </div>
        <div className="rounded-lg border border-border bg-card p-4">
          <p className="text-sm text-muted-foreground">Last event</p>
          <p className="mt-1 font-mono text-sm text-foreground">
            {state.systemStatus?.lastEventAt
              ? formatTimestamp(state.systemStatus.lastEventAt)
              : "—"}
          </p>
        </div>
        <div className="rounded-lg border border-border bg-card p-4">
          <p className="text-sm text-muted-foreground">Backend health</p>
          <p className="mt-1 font-mono text-lg capitalize text-foreground">
            {state.health?.status ?? "unknown"}
          </p>
        </div>
        <div className="rounded-lg border border-border bg-card p-4">
          <p className="text-sm text-muted-foreground">API correlation</p>
          <p className="mt-1 truncate font-mono text-sm text-foreground">
            {state.lastEvent?.correlationId ?? "—"}
          </p>
        </div>
      </div>

      <div className="mb-8 grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
        {services.map((service) => (
          <div key={service.key} className="rounded-lg border border-border bg-card p-4">
            <div className="mb-3 flex items-center justify-between">
              <span className="font-medium text-card-foreground">{service.name}</span>
              <div className="flex items-center gap-2">
                <div className={`size-2 rounded-full ${STATUS_COLORS[service.status]}`} />
                <span className="text-xs capitalize text-muted-foreground">{service.status}</span>
              </div>
            </div>
            <div className="flex items-center gap-6 text-sm">
              <div>
                <span className="text-muted-foreground">State</span>
                <p className="font-mono text-foreground">{service.state}</p>
              </div>
              <div>
                <span className="text-muted-foreground">{service.metricLabel}</span>
                <p className="font-mono text-foreground">{service.metricValue}</p>
              </div>
            </div>
          </div>
        ))}
      </div>

      <div className="rounded-lg border border-border bg-card">
        <div className="border-b border-border p-4">
          <div className="flex flex-col justify-between gap-4 sm:flex-row sm:items-center">
            <h2 className="font-medium text-card-foreground">Live events</h2>
            <div className="flex items-center gap-2">
              <div className="relative">
                <Search className="absolute top-1/2 left-3 size-4 -translate-y-1/2 text-muted-foreground" />
                <Input
                  placeholder="Search events..."
                  value={searchQuery}
                  onChange={(event) => setSearchQuery(event.target.value)}
                  className="h-8 w-48 border-0 bg-secondary pl-9"
                />
              </div>
              <DropdownMenu>
                <DropdownMenuTrigger asChild>
                  <Button variant="secondary" size="sm" className="h-8 gap-2">
                    {selectedService === "all" ? "All Services" : selectedService}
                    <ChevronDown className="size-3" />
                  </Button>
                </DropdownMenuTrigger>
                <DropdownMenuContent align="end">
                  <DropdownMenuItem onClick={() => setSelectedService("all")}>
                    All Services
                  </DropdownMenuItem>
                  {services.map((service) => (
                    <DropdownMenuItem
                      key={service.key}
                      onClick={() => setSelectedService(service.name)}
                    >
                      {service.name}
                    </DropdownMenuItem>
                  ))}
                </DropdownMenuContent>
              </DropdownMenu>
            </div>
          </div>

          <div className="mt-4 flex items-center gap-2">
            {(["all", "info", "warning", "error", "debug"] as const).map((level) => (
              <button
                key={level}
                type="button"
                onClick={() => setSelectedLevel(level)}
                className={`rounded-full px-3 py-1 text-xs font-medium transition-colors ${
                  selectedLevel === level
                    ? "bg-foreground text-background"
                    : "bg-secondary text-muted-foreground hover:text-foreground"
                }`}
              >
                {level === "all" ? "All" : level.charAt(0).toUpperCase() + level.slice(1)}
              </button>
            ))}
          </div>
        </div>

        <ScrollArea className="h-96">
          <div className="divide-y divide-border">
            {filteredLogs.length === 0 ? (
              <p className="py-8 text-center text-muted-foreground">
                {state.recentEvents.length === 0
                  ? "Waiting for live events…"
                  : "No events match the current filters"}
              </p>
            ) : (
              filteredLogs.map((log) => (
                <div key={log.id} className="flex items-start gap-4 px-4 py-3 text-sm">
                  <span className="font-mono text-xs whitespace-nowrap text-muted-foreground">
                    {formatLogTime(log.timestamp)}
                  </span>
                  <Badge variant="outline" className={`text-xs font-normal ${LEVEL_COLORS[log.level]}`}>
                    {log.level}
                  </Badge>
                  <span className="min-w-24 whitespace-nowrap text-muted-foreground">
                    {log.service}
                  </span>
                  <span className="text-foreground">{log.message}</span>
                </div>
              ))
            )}
          </div>
        </ScrollArea>
      </div>
    </main>
  );
}
