import { AppWindow, DoorOpen, Loader2, ShieldAlert, Square } from "lucide-react";

import { ModulePageHeader } from "@/components/module-page-header.js";
import { useSensorsModule } from "@/hooks/sensors-module-provider.js";
import { cn } from "@/lib/utils.js";
import type { ContactSensor, SensorType } from "@/types/sensors-types.js";

/** Contact sensors module artwork used across the Sensors UI. */
const SENSORS_MODULE_ICON = "/sensors-module.png?v=3";

/**
 * Icon for a sensor type.
 * @param props - Icon props.
 * @param props.type - Sensor classification.
 * @returns Icon element.
 */
function SensorTypeIcon({ type }: { type: SensorType }): React.JSX.Element {
  switch (type) {
    case "door":
      return <DoorOpen className="h-4 w-4" aria-hidden />;
    case "window":
      return <AppWindow className="h-4 w-4" aria-hidden />;
    default:
      return <Square className="h-4 w-4" aria-hidden />;
  }
}

/**
 * State pill for open/closed/unknown.
 * @param props - Pill props.
 * @param props.sensor - Sensor row.
 * @returns Status pill element.
 */
function StatePill({ sensor }: { sensor: ContactSensor }): React.JSX.Element {
  const label =
    sensor.contactState === "open"
      ? "Open"
      : sensor.contactState === "closed"
        ? "Closed"
        : "Unknown";

  const tone =
    sensor.contactState === "open"
      ? "bg-warning/15 text-foreground"
      : sensor.contactState === "closed"
        ? "bg-success/15 text-foreground"
        : "bg-secondary text-muted-foreground";

  return (
    <span className={cn("rounded-full px-2.5 py-0.5 text-xs font-medium", tone)}>
      {label}
    </span>
  );
}

/**
 * Contact sensors detail page with live grid.
 */
export function ContactSensorsPage(): React.JSX.Element {
  const { state, patchSensor } = useSensorsModule();
  const sensors = state.snapshot?.sensors ?? [];
  const openCount = sensors.filter((sensor) => sensor.contactState === "open").length;

  const headerSubtitle = state.loading
    ? "Loading sensors…"
    : `${sensors.length} sensors · ${openCount} open · ${state.sseConnected ? "live" : "reconnecting"}`;

  return (
    <main className="mx-auto max-w-4xl overflow-x-hidden px-4 py-8">
      <ModulePageHeader
        iconSrc={SENSORS_MODULE_ICON}
        title="Contact Sensors"
        subtitle={headerSubtitle}
      />

      {state.error ? (
        <p className="-mt-2 mb-6 text-sm text-destructive" role="alert">
          {state.error}
        </p>
      ) : null}

      <div className="grid gap-3 sm:grid-cols-2 lg:grid-cols-3">
        {sensors.map((sensor) => (
          <article
            key={sensor.id}
            className="rounded-lg border border-border bg-card p-4 shadow-sm"
          >
            <div className="flex items-start justify-between gap-2">
              <div className="min-w-0">
                <div className="flex items-center gap-2">
                  <SensorTypeIcon type={sensor.type} />
                  <h2 className="truncate font-medium text-foreground">{sensor.name}</h2>
                </div>
                <p className="mt-1 text-xs text-muted-foreground">
                  #{sensor.sensorNumber}
                  {sensor.roomName ? ` · ${sensor.roomName}` : ""}
                </p>
              </div>
              <StatePill sensor={sensor} />
            </div>

            {sensor.faulted ? (
              <div className="mt-3 flex items-center gap-1.5 text-xs text-destructive">
                <ShieldAlert className="h-3.5 w-3.5" aria-hidden />
                {sensor.faultReason ?? "Fault"}
              </div>
            ) : null}

            <div className="mt-4 flex items-center justify-between gap-2 border-t border-border pt-3">
              <label className="flex items-center gap-2 text-xs text-muted-foreground">
                <input
                  type="checkbox"
                  className="rounded border-border"
                  checked={sensor.homekitEnabled}
                  disabled={
                    !state.snapshot?.homekitBridgeReachable ||
                    state.savingSensorId === sensor.id
                  }
                  onChange={(event) => {
                    void patchSensor(sensor.id, { homekitEnabled: event.target.checked });
                  }}
                />
                HomeKit
              </label>
              {state.savingSensorId === sensor.id ? (
                <Loader2 className="h-4 w-4 animate-spin text-muted-foreground" aria-hidden />
              ) : null}
            </div>
          </article>
        ))}
      </div>
    </main>
  );
}
