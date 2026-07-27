import { useCallback, useEffect, useRef, useState } from "react";

import { getAppConfig } from "@/config/app-config.js";
import type { EventEnvelope } from "@/types/dashboard-types.js";
import type {
  ContactSensor,
  ContactSensorPatch,
  SensorsSnapshot,
} from "@/types/sensors-types.js";

/**
 * Live contact sensors module state.
 */
export interface SensorsModuleState {
  snapshot: SensorsSnapshot | null;
  loading: boolean;
  snapshotHydrated: boolean;
  error: string | null;
  savingSensorId: string | null;
  sseConnected: boolean;
}

const emptySnapshot: SensorsSnapshot = {
  module: "contact-sensors",
  sensorCount: 0,
  sensors: [],
  hardwareReady: false,
  homekitBridgeReachable: false,
  services: {
    sensors: "offline",
    homekitBridge: "offline",
  },
};

/**
 * Applies a broker SSE envelope to the sensors snapshot.
 * @param snapshot - Current snapshot.
 * @param envelope - SSE event envelope.
 * @returns Updated snapshot.
 */
function applySensorsEnvelope(
  snapshot: SensorsSnapshot,
  envelope: EventEnvelope
): SensorsSnapshot {
  if (
    envelope.event !== "contact_changed" &&
    envelope.event !== "contact_config_updated" &&
    envelope.event !== "contact_faulted"
  ) {
    return snapshot;
  }

  const payload = envelope.payload as { sensor?: ContactSensor };
  const sensor = payload.sensor;
  if (!sensor?.id) {
    return snapshot;
  }

  const sensors = snapshot.sensors.map((row) =>
    row.id === sensor.id ? { ...row, ...sensor } : row
  );

  return { ...snapshot, sensors };
}

/**
 * Provides live contact sensors state via REST bootstrap and SSE patches.
 * @returns Sensors module state and actions.
 */
export function useSensorsModuleState(): {
  state: SensorsModuleState;
  refresh: () => Promise<void>;
  patchSensor: (sensorId: string, patch: ContactSensorPatch) => Promise<void>;
} {
  const [state, setState] = useState<SensorsModuleState>({
    snapshot: null,
    loading: true,
    snapshotHydrated: false,
    error: null,
    savingSensorId: null,
    sseConnected: false,
  });
  const eventSourceRef = useRef<EventSource | null>(null);

  const loadSnapshot = useCallback(async (): Promise<void> => {
    const config = getAppConfig();
    try {
      const response = await fetch(`${config.apiBaseUrl}/api/contact-sensors`);
      if (!response.ok) {
        throw new Error(`Failed to load contact sensors (${response.status})`);
      }
      const body = (await response.json()) as { data?: SensorsSnapshot };
      const snapshot = body.data ?? emptySnapshot;
      setState((current) => ({
        ...current,
        snapshot,
        loading: false,
        snapshotHydrated: true,
        error: null,
      }));
    } catch (error) {
      setState((current) => ({
        ...current,
        loading: false,
        error: error instanceof Error ? error.message : "Failed to load contact sensors",
      }));
    }
  }, []);

  useEffect(() => {
    void loadSnapshot();
  }, [loadSnapshot]);

  useEffect(() => {
    const config = getAppConfig();
    const source = new EventSource(config.eventsUrl);

    source.onopen = () => {
      setState((current) => ({ ...current, sseConnected: true }));
    };

    source.onerror = () => {
      setState((current) => ({ ...current, sseConnected: false }));
    };

    source.onmessage = (message) => {
      let envelope: EventEnvelope;
      try {
        envelope = JSON.parse(message.data) as EventEnvelope;
      } catch {
        return;
      }

      setState((current) => {
        const base = current.snapshot ?? emptySnapshot;
        return {
          ...current,
          snapshot: applySensorsEnvelope(base, envelope),
        };
      });
    };

    eventSourceRef.current = source;
    return () => {
      source.close();
      eventSourceRef.current = null;
    };
  }, []);

  const patchSensor = useCallback(
    async (sensorId: string, patch: ContactSensorPatch): Promise<void> => {
      const config = getAppConfig();
      setState((current) => ({ ...current, savingSensorId: sensorId }));
      try {
        const response = await fetch(`${config.apiBaseUrl}/api/contact-sensors/${sensorId}`, {
          method: "PATCH",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            sensorName: patch.name,
            sensorType: patch.type,
            roomId: patch.roomId,
            roomName: patch.roomName,
            homekitEnabled: patch.homekitEnabled,
          }),
        });
        if (!response.ok) {
          throw new Error(`Failed to update sensor (${response.status})`);
        }
        const body = (await response.json()) as {
          data?: { sensor?: ContactSensor };
        };
        const updated = body.data?.sensor;
        if (updated) {
          setState((current) => {
            if (!current.snapshot) {
              return current;
            }
            const sensors = current.snapshot.sensors.map((row) =>
              row.id === updated.id ? { ...row, ...updated } : row
            );
            return {
              ...current,
              snapshot: { ...current.snapshot, sensors },
              savingSensorId: null,
            };
          });
        } else {
          setState((current) => ({ ...current, savingSensorId: null }));
        }
      } catch (error) {
        setState((current) => ({
          ...current,
          savingSensorId: null,
          error: error instanceof Error ? error.message : "Failed to update sensor",
        }));
      }
    },
    []
  );

  return {
    state,
    refresh: loadSnapshot,
    patchSensor,
  };
}
