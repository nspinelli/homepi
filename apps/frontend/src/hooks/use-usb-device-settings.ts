import { useCallback, useEffect, useMemo, useState } from "react";

import { getAppConfig } from "../config/app-config.js";
import type { ApiResponse, UsbAssignments, UsbDevice } from "../types/dashboard-types.js";

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
  save: () => Promise<void>;
  reload: () => Promise<void>;
} {
  const [devices, setDevices] = useState<UsbDevice[]>([]);
  const [saved, setSaved] = useState<UsbAssignments>(emptyAssignments);
  const [draft, setDraftState] = useState<UsbAssignments>(emptyAssignments);
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
      const [devicesRes, assignmentsRes] = await Promise.all([
        fetch(`${apiBaseUrl}/api/usb-devices`),
        fetch(`${apiBaseUrl}/api/usb-devices/assignments`),
      ]);

      const devicesJson = (await devicesRes.json()) as ApiResponse<{ devices: UsbDevice[] }>;
      const assignmentsJson = (await assignmentsRes.json()) as ApiResponse<UsbAssignments>;

      if (!devicesRes.ok || !devicesJson.ok) {
        throw new Error(devicesJson.error?.message ?? "Failed to load USB devices");
      }
      if (!assignmentsRes.ok || !assignmentsJson.ok) {
        throw new Error(assignmentsJson.error?.message ?? "Failed to load assignments");
      }

      const nextDevices = devicesJson.data?.devices ?? [];
      const nextAssignments = assignmentsJson.data ?? emptyAssignments;
      setDevices(nextDevices);
      setSaved(nextAssignments);
      setDraftState(nextAssignments);
    } catch (err) {
      setLoadError(err instanceof Error ? err.message : "Failed to load USB settings");
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void reload();
  }, [reload]);

  const setDraft = useCallback((patch: Partial<UsbAssignments>) => {
    setSaveSuccess(null);
    setSaveError(null);
    setDraftState((current) => ({ ...current, ...patch }));
  }, []);

  const save = useCallback(async () => {
    const { apiBaseUrl } = getAppConfig();
    setSaving(true);
    setSaveError(null);
    setSaveSuccess(null);
    try {
      const response = await fetch(`${apiBaseUrl}/api/usb-devices/assignments`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(draft),
      });
      const json = (await response.json()) as ApiResponse<UsbAssignments>;
      if (!response.ok || !json.ok) {
        throw new Error(json.error?.message ?? "Failed to save assignments");
      }
      const next = json.data ?? draft;
      setSaved(next);
      setDraftState(next);
      setSaveSuccess("Audio configuration saved successfully.");
    } catch (err) {
      setSaveError(err instanceof Error ? err.message : "Failed to save assignments");
    } finally {
      setSaving(false);
    }
  }, [draft]);

  const isDirty = useMemo(
    () =>
      saved.serial !== draft.serial ||
      saved.audioPrimary !== draft.audioPrimary ||
      saved.paging !== draft.paging,
    [saved, draft]
  );

  return {
    state: {
      devices,
      saved,
      draft,
      loading,
      saving,
      loadError,
      saveError,
      saveSuccess,
      isDirty,
    },
    setDraft,
    save,
    reload,
  };
}
