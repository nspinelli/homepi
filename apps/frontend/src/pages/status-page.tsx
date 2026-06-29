import { useCallback, useMemo, useState } from "react";
import { Link } from "react-router-dom";
import { ArrowLeft, Search } from "lucide-react";

import { ModulePageHeader } from "@/components/module-page-header.js";
import {
  ModuleHealthSection,
  PlatformHealthSection,
} from "@/components/status/module-health-section.js";
import {
  LazySection,
  ModuleSectionSkeleton,
  PlatformSectionSkeleton,
  StatusSectionsLoadingPlaceholder,
} from "@/components/status/lazy-section.js";
import { cn } from "@/lib/utils.js";
import { Badge } from "@/components/ui/badge.js";
import { Button } from "@/components/ui/button.js";
import { Input } from "@/components/ui/input.js";
import { ScrollArea } from "@/components/ui/scroll-area.js";
import {
  buildLogEntries,
  buildTransportCards,
  formatLogTime,
  formatTimestamp,
  formatUptime,
  formatCpuTemp,
  mapCpuTempStatus,
  type LogLevel,
  type ServiceVisualStatus,
} from "@/lib/status-display.js";
import { useSystemDashboard } from "@/hooks/system-dashboard-provider.js";

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
  const [selectedLevel, setSelectedLevel] = useState<LogLevel | "all">("info");
  const [selectedServices, setSelectedServices] = useState<Set<string>>(() => new Set());
  const [searchQuery, setSearchQuery] = useState("");

  const host = state.coreStatus?.host;
  const modules = state.coreStatus?.modules ?? [];
  const platform = state.coreStatus?.platform ?? [];

  const toggleServiceFilter = useCallback((serviceName: string) => {
    setSelectedServices((current) => {
      const next = new Set(current);
      if (next.has(serviceName)) {
        next.delete(serviceName);
      } else {
        next.add(serviceName);
      }
      return next;
    });
  }, []);

  const transportCards = useMemo(
    () => buildTransportCards({ sse: state.sseState, ws: state.wsState }),
    [state.sseState, state.wsState]
  );

  const logs = useMemo(() => buildLogEntries(state.recentEvents), [state.recentEvents]);

  const filteredLogs = logs.filter((log) => {
    const matchesLevel =
      selectedLevel === "all" || log.level === selectedLevel || (selectedLevel === "info" && log.level !== "debug");
    const matchesService =
      selectedServices.size === 0 || selectedServices.has(log.service);
    const matchesSearch =
      searchQuery === "" ||
      log.message.toLowerCase().includes(searchQuery.toLowerCase()) ||
      log.service.toLowerCase().includes(searchQuery.toLowerCase());
    return matchesLevel && matchesService && matchesSearch;
  });

  const isInitialModuleLoad = state.loading && modules.length === 0;
  const statusSubtitle = state.loading
    ? "Loading platform status…"
    : `${modules.length} modules · ${platform.length} platform services · ${filteredLogs.length} events shown`;

  return (
    <main className="mx-auto max-w-4xl overflow-x-hidden px-4 py-6">
      <Button variant="ghost" size="sm" className="mb-4 gap-2 text-muted-foreground" asChild>
        <Link to="/">
          <ArrowLeft className="size-4" />
          Back
        </Link>
      </Button>
      <ModulePageHeader
        iconSrc="/homepi-logo.png"
        iconAlt="HomePi"
        title="System Status"
        subtitle={statusSubtitle}
        actions={
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
        }
      />

      {state.error ? (
        <p className="mb-4 rounded-lg border border-destructive/30 bg-destructive/10 px-4 py-3 text-sm text-destructive">
          {state.error}
        </p>
      ) : null}

      {state.transportError ? (
        <p className="mb-4 rounded-lg border border-amber-500/30 bg-amber-500/10 px-4 py-3 text-sm text-amber-200">
          {state.transportError}
        </p>
      ) : null}

      {state.coreStatus?.healthServiceReachable === false ? (
        <p className="mb-4 rounded-lg border border-amber-500/30 bg-amber-500/10 px-4 py-3 text-sm text-amber-200">
          Health observer is unreachable
          {state.lastFetchedAt ? ` — last refresh ${formatTimestamp(state.lastFetchedAt)}` : ""}.
        </p>
      ) : null}

      <div className="mb-8 grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
        <div className="rounded-lg border border-border bg-card p-4">
          <p className="text-sm text-muted-foreground">Platform uptime</p>
          <p className="mt-1 font-mono text-lg text-foreground">
            {formatUptime(host?.uptimeMs ?? state.hostMetrics?.uptimeMs)}
          </p>
        </div>
        <div className="rounded-lg border border-border bg-card p-4">
          <p className="text-sm text-muted-foreground">CPU temperature</p>
          <div className="mt-1 flex items-center gap-2">
            <div
              className={`size-2 rounded-full ${STATUS_COLORS[mapCpuTempStatus(host?.cpuTempC ?? state.hostMetrics?.cpuTempC)]}`}
            />
            <p className="font-mono text-lg text-foreground">
              {formatCpuTemp(host?.cpuTempC ?? state.hostMetrics?.cpuTempC)}
            </p>
          </div>
        </div>
        <div className="rounded-lg border border-border bg-card p-4">
          <p className="text-sm text-muted-foreground">Last event</p>
          <p className="mt-1 font-mono text-sm text-foreground">
            {host?.lastEventAt ?? state.hostMetrics?.lastEventAt
              ? formatTimestamp(String(host?.lastEventAt ?? state.hostMetrics?.lastEventAt))
              : "—"}
          </p>
        </div>
      </div>

      <div
        className={cn(
          "mb-8 space-y-4 transition-opacity duration-300",
          state.loading && !isInitialModuleLoad && "opacity-60"
        )}
        aria-busy={state.loading}
      >
        {isInitialModuleLoad ? (
          <StatusSectionsLoadingPlaceholder />
        ) : (
          <>
            {modules.map((module) => (
              <LazySection key={module.module} skeleton={<ModuleSectionSkeleton />}>
                <ModuleHealthSection module={module} />
              </LazySection>
            ))}
            {platform.length > 0 ? (
              <LazySection skeleton={<PlatformSectionSkeleton />}>
                <PlatformHealthSection entries={platform} />
              </LazySection>
            ) : null}
          </>
        )}
      </div>

      <div className="mb-8 grid grid-cols-1 gap-4 sm:grid-cols-2">
        {transportCards.map((service) => (
          <button
            key={service.name}
            type="button"
            aria-pressed={selectedServices.has(service.name)}
            onClick={() => toggleServiceFilter(service.name)}
            className={`rounded-lg border bg-card p-4 text-left transition-colors ${
              selectedServices.has(service.name)
                ? "border-primary ring-2 ring-primary/40"
                : "border-border hover:border-muted-foreground/40"
            }`}
          >
            <div className="flex items-center justify-between">
              <span className="font-medium text-card-foreground">{service.name}</span>
              <div className="flex items-center gap-2">
                <div className={`size-2 rounded-full ${STATUS_COLORS[service.status]}`} />
                <span className="text-xs capitalize text-muted-foreground">{service.status}</span>
              </div>
            </div>
          </button>
        ))}
      </div>

      <div className="rounded-lg border border-border bg-card">
        <div className="border-b border-border p-4">
          <div className="flex flex-col justify-between gap-4 sm:flex-row sm:items-center">
            <h2 className="font-medium text-card-foreground">Activity log</h2>
            <div className="relative w-full sm:w-auto">
              <Search className="absolute top-1/2 left-3 size-4 -translate-y-1/2 text-muted-foreground" />
              <Input
                placeholder="Search events..."
                value={searchQuery}
                onChange={(event) => setSearchQuery(event.target.value)}
                className="h-8 w-full border-0 bg-secondary pl-9 sm:w-48"
              />
            </div>
          </div>

          <div className="mt-4 flex flex-wrap items-center gap-2">
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
                  ? "Waiting for meaningful activity…"
                  : "No events match the current filters"}
              </p>
            ) : (
              filteredLogs.map((log) => (
                <div
                  key={log.id}
                  className="flex flex-col gap-2 px-4 py-3 text-sm sm:flex-row sm:items-start sm:gap-4"
                >
                  <div className="flex min-w-0 flex-wrap items-center gap-2 sm:shrink-0">
                    <span className="font-mono text-xs text-muted-foreground">
                      {formatLogTime(log.timestamp)}
                    </span>
                    <Badge variant="outline" className={`text-xs font-normal ${LEVEL_COLORS[log.level]}`}>
                      {log.level}
                    </Badge>
                    <span className="text-muted-foreground">{log.service}</span>
                  </div>
                  <span className="min-w-0 break-words text-foreground">{log.message}</span>
                </div>
              ))
            )}
          </div>
        </ScrollArea>
      </div>
    </main>
  );
}
