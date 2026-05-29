import { CheckCircle2, CircleAlert, Loader2 } from "lucide-react";

import { Button } from "@/components/ui/button.js";
import { useUsbDeviceSettings } from "@/hooks/use-usb-device-settings.js";
import type { UsbDevice } from "@/types/dashboard-types.js";

/**
 * Props for a device role dropdown row.
 */
interface DeviceSelectRowProps {
  /** Field label. */
  label: string;
  /** Selected device id. */
  value: string | null;
  /** Options filtered for this role. */
  options: UsbDevice[];
  /** Change handler. */
  onChange: (deviceId: string | null) => void;
  /** Disable interaction. */
  disabled?: boolean;
}

/**
 * Renders a labeled device dropdown.
 * @param props - Row props.
 * @returns Select row element.
 */
function DeviceSelectRow({
  label,
  value,
  options,
  onChange,
  disabled = false,
}: DeviceSelectRowProps): React.JSX.Element {
  return (
    <label className="grid gap-2">
      <span className="text-sm font-medium text-foreground">{label}</span>
      <select
        className="h-10 w-full rounded-md border border-border bg-background px-3 text-sm text-foreground"
        value={value ?? ""}
        disabled={disabled}
        onChange={(event) => onChange(event.target.value || null)}
      >
        <option value="">Not assigned</option>
        {options.map((device) => (
          <option key={device.deviceId} value={device.deviceId} disabled={!device.present}>
            {device.displayName}
            {!device.present ? " (offline)" : ""}
          </option>
        ))}
      </select>
    </label>
  );
}

/**
 * Audio Configuration settings card for USB role assignments.
 */
export function AudioConfigurationCard(): React.JSX.Element {
  const { state, setDraft, save } = useUsbDeviceSettings();

  const pluggedIn = state.devices.filter((device) => device.present);
  const serialOptions = pluggedIn.filter((device) => device.kind === "serial");
  const audioOptions = pluggedIn.filter((device) => device.kind === "audio");

  return (
    <div className="rounded-lg border border-border bg-card">
      <div className="border-b border-border px-6 py-4">
        <h2 className="font-medium text-card-foreground">Audio Configuration</h2>
        <p className="mt-0.5 text-sm text-muted-foreground">
          Assign USB serial and audio devices for HomePi services
        </p>
      </div>

      <div className="grid gap-4 p-6">
        {state.loadError ? (
          <div
            className="flex items-start gap-2 rounded-md border border-destructive/40 bg-destructive/10 px-3 py-2 text-sm text-destructive"
            role="alert"
          >
            <CircleAlert className="mt-0.5 size-4 shrink-0" aria-hidden />
            <p>{state.loadError}</p>
          </div>
        ) : null}

        {state.saveSuccess ? (
          <div
            className="flex items-start gap-2 rounded-md border border-emerald-500/40 bg-emerald-500/10 px-3 py-2 text-sm text-emerald-700 dark:text-emerald-400"
            role="status"
          >
            <CheckCircle2 className="mt-0.5 size-4 shrink-0" aria-hidden />
            <p>{state.saveSuccess}</p>
          </div>
        ) : null}

        {state.saveError ? (
          <div
            className="flex items-start gap-2 rounded-md border border-destructive/40 bg-destructive/10 px-3 py-2 text-sm text-destructive"
            role="alert"
          >
            <CircleAlert className="mt-0.5 size-4 shrink-0" aria-hidden />
            <p>{state.saveError}</p>
          </div>
        ) : null}

        <DeviceSelectRow
          label="Primary Serial Connection"
          value={state.draft.serial}
          options={serialOptions}
          disabled={state.loading || state.saving}
          onChange={(deviceId) => setDraft({ serial: deviceId })}
        />

        <DeviceSelectRow
          label="Primary Audio Output"
          value={state.draft.audioPrimary}
          options={audioOptions}
          disabled={state.loading || state.saving}
          onChange={(deviceId) => setDraft({ audioPrimary: deviceId })}
        />

        <DeviceSelectRow
          label="Primary Paging Output"
          value={state.draft.paging}
          options={audioOptions}
          disabled={state.loading || state.saving}
          onChange={(deviceId) => setDraft({ paging: deviceId })}
        />

        <div className="flex flex-col items-end gap-2 pt-2 sm:flex-row sm:items-center sm:justify-end">
          {!state.saveSuccess && !state.saveError && state.isDirty && !state.saving ? (
            <p className="text-sm text-muted-foreground sm:mr-auto">You have unsaved changes</p>
          ) : null}
          <Button
            type="button"
            disabled={!state.isDirty || state.loading || state.saving}
            onClick={() => void save()}
          >
            {state.saving ? (
              <>
                <Loader2 className="mr-2 size-4 animate-spin" />
                Saving…
              </>
            ) : (
              "Save"
            )}
          </Button>
        </div>
      </div>
    </div>
  );
}
