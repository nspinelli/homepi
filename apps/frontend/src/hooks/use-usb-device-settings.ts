import { useCallback, useEffect, useMemo, useRef, useState } from "react";

import { getAppConfig } from "../config/app-config.js";
import type {
  ApiResponse,
  AudioCapabilities,
  AudioProfileTuple,
  EventEnvelope,
  UsbAssignments,
  UsbDevice,
} from "../types/dashboard-types.js";

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
  /** Supported profile tuples for the selected primary audio device. */
  supportedProfileTuples: AudioProfileTuple[];
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
  /** Whether save is blocked pending profile selection. */
  profileSelectionRequired: boolean;
  /** Blocking alert when the saved profile is invalid or paused. */
  profileAlert: string | null;
}

const emptyAssignments: UsbAssignments = {
  serial: null,
  audioPrimary: null,
  paging: null,
  audioPrimaryProfile: null,
};

/**
 * Formats a profile tuple for display.
 * @param tuple - Profile tuple.
 * @returns Human-readable label.
 */
export function formatProfileTuple(tuple: AudioProfileTuple): string {
  const channels = tuple.channels === 1 ? "Mono" : "Stereo";
  return `${tuple.sampleRate} Hz · ${tuple.sampleFormat} · ${channels}`;
}

/**
 * Waits briefly for a fetch response, retrying transient service restarts.
 * @param url - Request URL.
 * @param init - Fetch init options.
 * @param attempts - Maximum attempts.
 * @returns Successful response.
 */
async function fetchWithServiceRetry(
  url: string,
  init: RequestInit,
  attempts = 15
): Promise<Response> {
  let lastError: Error | null = null;
  for (let attempt = 0; attempt < attempts; attempt += 1) {
    try {
      const response = await fetch(url, init);
      const clone = response.clone();
      const body = (await clone.json()) as ApiResponse<unknown>;
      const message = body.error?.message ?? "";
      const transient =
        message.includes("ECONNREFUSED") ||
        message.includes("AUDIO_UNAVAILABLE") ||
        response.status === 503;
      if (!transient || response.ok) {
        return response;
      }
      lastError = new Error(message || `Request failed with status ${response.status}`);
    } catch (error) {
      lastError = error instanceof Error ? error : new Error("Request failed");
    }
    await new Promise((resolve) => setTimeout(resolve, 1000));
  }
  throw lastError ?? new Error("Request failed");
}

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
  const [supportedProfileTuples, setSupportedProfileTuples] = useState<AudioProfileTuple[]>([]);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [saveError, setSaveError] = useState<string | null>(null);
  const [saveSuccess, setSaveSuccess] = useState<string | null>(null);
  const [profileAlert, setProfileAlert] = useState<string | null>(null);
  const eventSourceRef = useRef<EventSource | null>(null);

  const loadCapabilities = useCallback(async (deviceId: string | null) => {
    if (!deviceId) {
      setSupportedProfileTuples([]);
      return;
    }
    const { apiBaseUrl } = getAppConfig();
    const response = await fetch(
      `${apiBaseUrl}/api/usb-devices/${encodeURIComponent(deviceId)}/audio-capabilities`
    );
    const json = (await response.json()) as ApiResponse<AudioCapabilities>;
    if (!response.ok || !json.ok || !json.data) {
      setSupportedProfileTuples([]);
      return;
    }
    setSupportedProfileTuples(json.data.supportedProfileTuples ?? []);
  }, []);

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
      await loadCapabilities(nextAssignments.audioPrimary);
    } catch (err) {
      setLoadError(err instanceof Error ? err.message : "Failed to load audio settings");
    } finally {
      setLoading(false);
    }
  }, [loadCapabilities]);

  useEffect(() => {
    void reload();
  }, [reload]);

  useEffect(() => {
    const { eventsUrl } = getAppConfig();
    const source = new EventSource(eventsUrl);
    eventSourceRef.current = source;

    source.addEventListener("envelope", (event) => {
      try {
        const envelope = JSON.parse(event.data) as EventEnvelope;
        if (envelope.source !== "homepi-usb-devices") {
          return;
        }
        if (
          envelope.event === "audio_profile_paused" ||
          envelope.event === "audio_profile_invalid"
        ) {
          setProfileAlert(
            "The saved audio profile is no longer valid. Select a supported profile and save again."
          );
          void reload();
        } else if (envelope.event === "audio_operating_profile_changed") {
          setProfileAlert(null);
        }
      } catch {
        /* ignore malformed events */
      }
    });

    return () => {
      source.close();
      eventSourceRef.current = null;
    };
  }, [reload]);

  const setDraft = useCallback(
    (patch: Partial<UsbAssignments>) => {
      setDraftState((current) => {
        const next = { ...current, ...patch };
        if (patch.audioPrimary !== undefined && patch.audioPrimary !== current.audioPrimary) {
          next.audioPrimaryProfile = null;
          void loadCapabilities(patch.audioPrimary);
        }
        if (patch.audioPrimary === null) {
          next.audioPrimaryProfile = null;
          setSupportedProfileTuples([]);
        }
        return next;
      });
      setSaveSuccess(null);
      setSaveError(null);
    },
    [loadCapabilities]
  );

  const profileSelectionRequired = Boolean(draft.audioPrimary && !draft.audioPrimaryProfile);

  const isDirty = useMemo(() => {
    const profileChanged =
      JSON.stringify(draft.audioPrimaryProfile ?? null) !==
      JSON.stringify(saved.audioPrimaryProfile ?? null);
    return (
      draft.serial !== saved.serial ||
      draft.audioPrimary !== saved.audioPrimary ||
      draft.paging !== saved.paging ||
      profileChanged
    );
  }, [draft, saved]);

  const save = useCallback(async () => {
    if (profileSelectionRequired) {
      setSaveError("Select an audio profile for the primary DAC before saving.");
      return;
    }

    const { apiBaseUrl } = getAppConfig();
    setSaving(true);
    setSaveError(null);
    setSaveSuccess(null);
    try {
      const assignmentsRes = await fetchWithServiceRetry(
        `${apiBaseUrl}/api/usb-devices/assignments`,
        {
          method: "PUT",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(draft),
        }
      );
      const assignmentsJson = (await assignmentsRes.json()) as ApiResponse<UsbAssignments>;

      if (!assignmentsRes.ok || !assignmentsJson.ok) {
        throw new Error(assignmentsJson.error?.message ?? "Failed to save assignments");
      }

      const nextSaved = assignmentsJson.data ?? draft;
      setSaved(nextSaved);
      setDraftState(nextSaved);
      setProfileAlert(null);
      setSaveSuccess("Audio configuration saved.");
    } catch (err) {
      setSaveError(err instanceof Error ? err.message : "Failed to save audio settings");
    } finally {
      setSaving(false);
    }
  }, [draft, profileSelectionRequired]);

  return {
    state: {
      devices,
      saved,
      draft,
      supportedProfileTuples,
      loading,
      saving,
      loadError,
      saveError,
      saveSuccess,
      isDirty,
      profileSelectionRequired,
      profileAlert,
    },
    setDraft,
    save,
    reload,
  };
}
