import { CircleAlert, ExternalLink, Loader2 } from "lucide-react";

import { Badge } from "@/components/ui/badge.js";
import { Button } from "@/components/ui/button.js";
import { Switch } from "@/components/ui/switch.js";
import type { UsePagingModuleResult } from "@/hooks/use-paging-module.js";
import { useUsbDeviceSettings } from "@/hooks/use-usb-device-settings.js";
import { cn } from "@/lib/utils.js";

/**
 * Props for paging config/readiness card.
 */
export interface PagingConfigCardProps {
  /** Shared paging hook result. */
  paging: UsePagingModuleResult;
  /** Opens Audio Settings tab where DAC assignment is configured. */
  onOpenSettings: () => void;
}

/**
 * Returns status styling for readiness badges.
 * @param active - Whether the dependency is healthy.
 * @returns Tailwind class list.
 */
function readinessBadgeClass(active: boolean): string {
  return active
    ? "border-emerald-500/30 bg-emerald-500/10 text-emerald-700 dark:text-emerald-400"
    : "border-amber-500/30 bg-amber-500/10 text-amber-800 dark:text-amber-300";
}

/**
 * Paging config card for enable/disable and runtime readiness.
 * Primary Paging DAC stays read-only and links to Settings.
 * @param props - Card props.
 * @returns Paging config card element.
 */
export function PagingConfigCard({
  paging,
  onOpenSettings,
}: PagingConfigCardProps): React.JSX.Element {
  const { state, setEnabled } = paging;
  const usb = useUsbDeviceSettings();
  const config = state.config;
  const status = state.status;
  const readiness = config?.status;
  const primaryPagingDeviceId = usb.state.saved.paging ?? null;
  const pagingDevice = usb.state.devices.find((device) => device.deviceId === primaryPagingDeviceId);
  const deviceLabel = pagingDevice?.displayName ?? primaryPagingDeviceId ?? "Not assigned";
  const isBusy = Boolean(state.pendingActions.updateConfig);

  const statusFlags = [
    {
      label: "resourceState",
      value: readiness?.resourceState ?? status?.resourceState ?? "unknown",
      active:
        (readiness?.resourceState ?? status?.resourceState ?? "COLD") === "WARM" ||
        (readiness?.resourceState ?? status?.resourceState ?? "COLD") === "ACTIVE",
    },
    {
      label: "dacConnected",
      value: String(
        readiness?.dacConnected ?? status?.dependencies.pagingDacConnected ?? false
      ),
      active: readiness?.dacConnected ?? status?.dependencies.pagingDacConnected ?? false,
    },
    {
      label: "dacOpen",
      value: String(readiness?.dacOpen ?? status?.dependencies.dacOpen ?? false),
      active: readiness?.dacOpen ?? status?.dependencies.dacOpen ?? false,
    },
    {
      label: "voiceLoaded",
      value: String(readiness?.voiceLoaded ?? status?.dependencies.ttsWorkerReady ?? false),
      active: readiness?.voiceLoaded ?? status?.dependencies.ttsWorkerReady ?? false,
    },
    {
      label: "hifiConnected",
      value: String(readiness?.hifiConnected ?? status?.dependencies.hifiConnected ?? false),
      active: readiness?.hifiConnected ?? status?.dependencies.hifiConnected ?? false,
    },
    {
      label: "busy",
      value: String(readiness?.busy ?? status?.busy ?? false),
      active: !(readiness?.busy ?? status?.busy ?? false),
    },
  ];

  return (
    <div className="rounded-lg border border-border bg-card">
      <div className="border-b border-border px-6 py-4">
        <h2 className="font-medium text-card-foreground">Paging Configuration</h2>
        <p className="mt-0.5 text-sm text-muted-foreground">
          Enable paging and confirm worker readiness for the assigned paging DAC.
        </p>
      </div>

      <div className="grid gap-4 p-6">
        {state.error ? (
          <div
            className="flex items-start gap-2 rounded-md border border-destructive/40 bg-destructive/10 px-3 py-2 text-sm text-destructive"
            role="alert"
          >
            <CircleAlert className="mt-0.5 size-4 shrink-0" aria-hidden />
            <p>{state.error}</p>
          </div>
        ) : null}

        <div className="flex items-center justify-between rounded-md border border-border/60 px-3 py-2">
          <div className="grid gap-0.5">
            <p className="text-sm font-medium text-foreground">Enable Paging</p>
            <p className="text-xs text-muted-foreground">
              Toggle paging APIs and worker lifecycle without changing DAC assignment.
            </p>
          </div>
          <Switch
            checked={config?.enabled ?? false}
            disabled={state.loadingConfig || isBusy}
            aria-label="Enable paging"
            onCheckedChange={(checked) => {
              void setEnabled(checked);
            }}
          />
        </div>

        <div className="rounded-md border border-border/60 px-3 py-3">
          <div className="flex items-start justify-between gap-2">
            <div className="grid gap-0.5">
              <p className="text-sm font-medium text-foreground">Primary Paging DAC</p>
              <p className="text-xs text-muted-foreground">{deviceLabel}</p>
            </div>
            <Button type="button" variant="outline" size="sm" onClick={onOpenSettings}>
              <ExternalLink className="mr-1 size-3.5" />
              Settings
            </Button>
          </div>
          <p className="mt-2 text-xs text-muted-foreground">
            DAC assignment is managed only in Audio Settings.
          </p>
        </div>

        <div className="grid gap-2">
          <p className="text-sm font-medium text-foreground">Readiness</p>
          <div className="flex flex-wrap gap-2">
            {statusFlags.map((pill) => (
              <Badge key={pill.label} variant="outline" className={cn(readinessBadgeClass(pill.active))}>
                {pill.label}: {pill.value}
              </Badge>
            ))}
            {(state.loadingConfig || state.loadingStatus) && !isBusy ? (
              <Badge variant="outline" className="border-border text-muted-foreground">
                <Loader2 className="size-3 animate-spin" />
                Loading
              </Badge>
            ) : null}
          </div>
        </div>
      </div>
    </div>
  );
}
