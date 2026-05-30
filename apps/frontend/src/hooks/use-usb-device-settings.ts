import { useCallback, useEffect, useMemo, useState } from "react";

import { getAppConfig } from "../config/app-config.js";
import type { ApiResponse, UsbAssignments, UsbDevice } from "../types/dashboard-types.js";

/**
 * HiFi source option for AirPlay mapping.
 */
export interface HifiSourceOption {
  /** Source slot number 1-8. */
  sourceNumber: number;
  /** Optional display name. */
  name?: string;
}

/**
 * State for the Audio Configuration settings panel.
 */
export interface UsbDeviceSettingsState {
  /** Available devices from the API. */
  devices: UsbDevice[];
  /** Saved assignments from the server. */
  saved: UsbAssignments;
  /** Draft assignments edited in the UI. */
  draft: UsbAssignments;
  /** Available HiFi sources. */
  sources: HifiSourceOption[];
  /** Saved AirPlay source number. */
  savedAirplaySource: number | null;
  /** Draft AirPlay source number. */
  draftAirplaySource: number | null;
  /** Whether data is loading. */
  loading: boolean;
  /** Whether a save is in progress. */
  saving: boolean;
  /** Load failure message. */
  loadError: string | null;
  /** Save failure message. */
  saveError: string | null;
  /** Save success message after a completed save. */
  saveSuccess: string | null;
  /** Whether draft differs from saved assignments. */
  isDirty: boolean;
}

const emptyAssignments: UsbAssignments = {
  serial: null,
  audioPrimary: null,
  paging: null,
};

/**
 * Loads USB devices and assignments and exposes save handling.
 * @returns Settings state and actions.
 */
export function useUsbDeviceSettings(): {
  state: UsbDeviceSettingsState;
  setDraft: (patch: Partial<UsbAssignments>) => void;
  setAirplaySource: (sourceNumber: number | null) => void;
  save: () => Promise<void>;
  reload: () => Promise<void>;
} {
  const [devices, setDevices] = useState<UsbDevice[]>([]);
  const [saved, setSaved] = useState<UsbAssignments>(emptyAssignments);
  const [draft, setDraftState] = useState<UsbAssignments>(emptyAssignments);
  const [sources, setSources] = useState<HifiSourceOption[]>([]);
  const [savedAirplaySource, setSavedAirplaySource] = useState<number | null>(5);
  const [draftAirplaySource, setDraftAirplaySource] = useState<number | null>(5);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [saveError, setSaveError] = useState<string | null>(null);
  const [saveSuccess, setSaveSuccess] = useState<string | null>(null);

  const reload = useCallback(async () => {
    const { apiBaseUrl } = getAppConfig();
    setLoading(true);
    setLoadError(null);
    try {
      const [devicesRes, assignmentsRes, sourcesRes, airplayRes] = await Promise.all([
        fetch(`${apiBaseUrl}/api/usb-devices`),
        fetch(`${apiBaseUrl}/api/usb-devices/assignments`),
        fetch(`${apiBaseUrl}/api/hifi-serial/sources`),
        fetch(`${apiBaseUrl}/api/audio/airplay-source`),
      ]);

      const devicesJson = (await devicesRes.json()) as ApiResponse<{ devices: UsbDevice[] }>;
      const assignmentsJson = (await assignmentsRes.json()) as ApiResponse<UsbAssignments>;
      const sourcesJson = (await sourcesRes.json()) as ApiResponse<{
        sources: Array<{ sourceNumber: number; name?: string }>;
      }>;
      const airplayJson = (await airplayRes.json()) as ApiResponse<{
        sourceNumber: number | null;
      }>;

      if (!devicesRes.ok || !devicesJson.ok) {
        throw new Error(devicesJson.error?.message ?? "Failed to load USB devices");
      }
      if (!assignmentsRes.ok || !assignmentsJson.ok) {
        throw new Error(assignmentsJson.error?.message ?? "Failed to load assignments");
      }

      const nextDevices = devicesJson.data?.devices ?? [];
      const nextAssignments = assignmentsJson.data ?? emptyAssignments;
      const nextSources =
        sourcesJson.ok && sourcesJson.data?.sources
          ? sourcesJson.data.sources.map((source) => ({
              sourceNumber: source.sourceNumber,
              name: source.name,
            }))
          : Array.from({ length: 8 }, (_, index) => ({ sourceNumber: index + 1 }));
      const nextAirplay =
        airplayJson.ok && airplayJson.data ? airplayJson.data.sourceNumber ?? 5 : 5;

      setDevices(nextDevices);
      setSaved(nextAssignments);
      setDraftState(nextAssignments);
      setSources(nextSources);
      setSavedAirplaySource(nextAirplay);
      setDraftAirplaySource(nextAirplay);
    } catch (err) {
      setLoadError(err instanceof Error ? err.message : "Failed to load audio settings");
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void reload();
  }, [reload]);

  const setDraft = useCallback((patch: Partial<UsbAssignments>) => {
    setDraftState((current) => ({ ...current, ...patch }));
    setSaveSuccess(null);
    setSaveError(null);
  }, []);

  const setAirplaySource = useCallback((sourceNumber: number | null) => {
    setDraftAirplaySource(sourceNumber);
    setSaveSuccess(null);
    setSaveError(null);
  }, []);

  const isDirty = useMemo(() => {
    return (
      draft.serial !== saved.serial ||
      draft.audioPrimary !== saved.audioPrimary ||
      draft.paging !== saved.paging ||
      draftAirplaySource !== savedAirplaySource
    );
  }, [draft, saved, draftAirplaySource, savedAirplaySource]);

  const save = useCallback(async () => {
    const { apiBaseUrl } = getAppConfig();
    setSaving(true);
    setSaveError(null);
    setSaveSuccess(null);
    try {
      const [assignmentsRes, airplayRes] = await Promise.all([
        fetch(`${apiBaseUrl}/api/usb-devices/assignments`, {
          method: "PUT",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(draft),
        }),
        draftAirplaySource !== null
          ? fetch(`${apiBaseUrl}/api/audio/airplay-source`, {
              method: "PUT",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({ sourceNumber: draftAirplaySource }),
            })
          : Promise.resolve(new Response(JSON.stringify({ ok: true }), { status: 200 })),
      ]);

      const assignmentsJson = (await assignmentsRes.json()) as ApiResponse<UsbAssignments>;
      const airplayJson = (await airplayRes.json()) as ApiResponse<{ sourceNumber: number }>;

      if (!assignmentsRes.ok || !assignmentsJson.ok) {
        throw new Error(assignmentsJson.error?.message ?? "Failed to save assignments");
      }
      if (!airplayRes.ok || !airplayJson.ok) {
        throw new Error(airplayJson.error?.message ?? "Failed to save AirPlay source");
      }

      setSaved(assignmentsJson.data ?? draft);
      setSavedAirplaySource(draftAirplaySource);
      setSaveSuccess("Audio configuration saved.");
    } catch (err) {
      setSaveError(err instanceof Error ? err.message : "Failed to save audio settings");
    } finally {
      setSaving(false);
    }
  }, [draft, draftAirplaySource]);

  return {
    state: {
      devices,
      saved,
      draft,
      sources,
      savedAirplaySource,
      draftAirplaySource,
      loading,
      saving,
      loadError,
      saveError,
      saveSuccess,
      isDirty,
    },
    setDraft,
    setAirplaySource,
    save,
    reload,
  };
}
