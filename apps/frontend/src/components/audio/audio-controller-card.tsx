import { useCallback, useEffect, useMemo, useState } from "react";
import { CircleAlert, Loader2, RefreshCw } from "lucide-react";

import { Button } from "@/components/ui/button.js";
import { Input } from "@/components/ui/input.js";
import { Label } from "@/components/ui/label.js";
import { useAudioModule } from "@/hooks/audio-module-provider.js";
import { formatTimestamp } from "@/lib/status-display.js";

/**
 * Props for a read-only controller form field.
 */
interface ControllerReadOnlyFieldProps {
  /** Field label. */
  label: string;
  /** Display value. */
  value: string;
}

/**
 * Renders a read-only controller form field.
 * @param props - Field props.
 * @returns Read-only field element.
 */
function ControllerReadOnlyField({
  label,
  value,
}: ControllerReadOnlyFieldProps): React.JSX.Element {
  return (
    <label className="grid gap-1.5">
      <Label>{label}</Label>
      <Input value={value} readOnly disabled className="bg-muted/40" />
    </label>
  );
}

/**
 * Formats an optional controller value for display.
 * @param value - Raw field value.
 * @param fallback - Fallback when empty.
 * @returns Display string.
 */
function formatOptionalValue(value: string | number | undefined, fallback = "—"): string {
  if (value === undefined || value === null || value === "") {
    return fallback;
  }
  return String(value);
}

/**
 * Formats a boolean-ish controller flag for display.
 * @param value - 0/1 flag.
 * @returns Yes, No, or em dash.
 */
function formatFlag(value: number | undefined): string {
  if (value === 1) {
    return "Yes";
  }
  if (value === 0) {
    return "No";
  }
  return "—";
}

/**
 * Formats an ISO timestamp for display.
 * @param value - ISO timestamp.
 * @returns Locale formatted timestamp or em dash.
 */
function formatOptionalTimestamp(value: string | undefined): string {
  if (!value) {
    return "—";
  }
  const parsed = new Date(value);
  if (Number.isNaN(parsed.getTime())) {
    return value;
  }
  return formatTimestamp(value);
}

/**
 * Controller settings card for the audio settings tab.
 */
export function AudioControllerCard(): React.JSX.Element {
  const { state, saveControllerSettings, syncController } = useAudioModule();
  const controller = state.snapshot?.controller;
  const [deviceName, setDeviceName] = useState(controller?.deviceName ?? "");
  const [actionError, setActionError] = useState<string | null>(null);

  useEffect(() => {
    setDeviceName(controller?.deviceName ?? "");
  }, [controller?.deviceName]);

  const isDirty = useMemo(() => {
    return deviceName.trim() !== (controller?.deviceName ?? "").trim();
  }, [controller?.deviceName, deviceName]);

  const handleSave = useCallback(async () => {
    const trimmed = deviceName.trim();
    if (!trimmed) {
      setActionError("Name is required.");
      return;
    }

    setActionError(null);
    try {
      await saveControllerSettings(trimmed);
    } catch (error) {
      setActionError(error instanceof Error ? error.message : "Save failed");
    }
  }, [deviceName, saveControllerSettings]);

  const handleSync = useCallback(async () => {
    setActionError(null);
    try {
      await syncController();
    } catch (error) {
      setActionError(error instanceof Error ? error.message : "Sync failed");
    }
  }, [syncController]);

  return (
    <div className="rounded-lg border border-border bg-card">
      <div className="border-b border-border px-6 py-4">
        <h2 className="font-medium text-card-foreground">Controller</h2>
        <p className="mt-0.5 text-sm text-muted-foreground">
          Hi-Fi2 controller details, name, and sync status.
        </p>
      </div>

      <div className="grid gap-4 p-6">
        {actionError ? (
          <div
            className="flex items-start gap-2 rounded-md border border-destructive/40 bg-destructive/10 px-3 py-2 text-sm text-destructive"
            role="alert"
          >
            <CircleAlert className="mt-0.5 size-4 shrink-0" aria-hidden />
            <p>{actionError}</p>
          </div>
        ) : null}

        <div className="grid gap-3 sm:grid-cols-2">
          <label className="grid gap-1.5 sm:col-span-2">
            <Label htmlFor="controller-name">Name</Label>
            <Input
              id="controller-name"
              value={deviceName}
              disabled={state.savingController || state.syncingController}
              onChange={(event) => setDeviceName(event.target.value)}
            />
          </label>

          <ControllerReadOnlyField
            label="Firmware Version"
            value={formatOptionalValue(controller?.firmwareVersion)}
          />
          <ControllerReadOnlyField
            label="Hardware Version"
            value={formatOptionalValue(controller?.hardwareVersion)}
          />
          <ControllerReadOnlyField
            label="MAC Address"
            value={formatOptionalValue(controller?.macAddress)}
          />
          <ControllerReadOnlyField
            label="IP Address"
            value={formatOptionalValue(controller?.ipAddress)}
          />
          <ControllerReadOnlyField
            label="Subnet Mask"
            value={formatOptionalValue(controller?.subnetMask)}
          />
          <ControllerReadOnlyField
            label="Gateway"
            value={formatOptionalValue(controller?.gateway)}
          />
          <ControllerReadOnlyField
            label="TCP Port"
            value={formatOptionalValue(controller?.tcpPort)}
          />
          <ControllerReadOnlyField label="DHCP Enabled" value={formatFlag(controller?.dhcpEnabled)} />
          <ControllerReadOnlyField
            label="Last Full Sync"
            value={formatOptionalTimestamp(controller?.lastFullSyncAt)}
          />
          <ControllerReadOnlyField
            label="Last Updated"
            value={formatOptionalTimestamp(controller?.updatedAt)}
          />
        </div>

        <div className="flex flex-col gap-3 border-t border-border pt-4 sm:flex-row sm:items-center sm:justify-between">
          <Button
            type="button"
            variant="outline"
            disabled={state.syncingController || state.savingController}
            onClick={() => void handleSync()}
          >
            {state.syncingController ? (
              <>
                <Loader2 className="mr-2 size-4 animate-spin" />
                Syncing…
              </>
            ) : (
              <>
                <RefreshCw className="mr-2 size-4" />
                Re-sync Controller
              </>
            )}
          </Button>

          <div className="flex flex-col items-end gap-2 sm:flex-row sm:items-center">
            {isDirty && !state.savingController ? (
              <p className="text-sm text-muted-foreground sm:mr-auto">You have unsaved changes</p>
            ) : null}
            {isDirty ? (
              <Button
                type="button"
                disabled={state.savingController || state.syncingController}
                onClick={() => void handleSave()}
              >
                {state.savingController ? (
                  <>
                    <Loader2 className="mr-2 size-4 animate-spin" />
                    Saving…
                  </>
                ) : (
                  "Save"
                )}
              </Button>
            ) : null}
          </div>
        </div>
      </div>
    </div>
  );
}
