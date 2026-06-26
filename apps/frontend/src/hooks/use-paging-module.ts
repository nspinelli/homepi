import { useCallback, useEffect, useMemo, useState } from "react";

import { getAppConfig } from "@/config/app-config.js";
import type { ApiResponse } from "@/types/dashboard-types.js";
import type {
  PagingApiKeyInfo,
  PagingApiKeySetResult,
  PagingChime,
  PagingConfig,
  PagingConfigPatch,
  PagingStatus,
  PagingVoice,
} from "@/types/paging-types.js";

/**
 * In-memory paging UI state and async operation flags.
 */
export interface PagingModuleState {
  config: PagingConfig | null;
  status: PagingStatus | null;
  apiKeyInfo: PagingApiKeyInfo | null;
  voices: PagingVoice[];
  chimes: PagingChime[];
  loadingConfig: boolean;
  loadingStatus: boolean;
  loadingApiKeyInfo: boolean;
  loadingVoices: boolean;
  loadingChimes: boolean;
  pendingActions: Record<string, boolean>;
  error: string | null;
}

/**
 * Shape exposed by `usePagingModule`.
 */
export interface UsePagingModuleResult {
  state: PagingModuleState;
  refreshAll: () => Promise<void>;
  refreshConfig: () => Promise<void>;
  refreshStatus: () => Promise<void>;
  refreshApiKeyInfo: () => Promise<void>;
  refreshVoices: () => Promise<void>;
  refreshChimes: () => Promise<void>;
  updateConfig: (patch: PagingConfigPatch) => Promise<void>;
  setEnabled: (enabled: boolean) => Promise<void>;
  setIdlePolicy: (policy: PagingConfigPatch["idlePolicy"]) => Promise<void>;
  setDefaultVoice: (voiceId: string) => Promise<void>;
  setApiKey: (apiKey: string) => Promise<PagingApiKeySetResult>;
  clearApiKey: () => Promise<void>;
  installVoice: (voiceId: string, setDefault: boolean) => Promise<void>;
  removeVoice: (voiceId: string) => Promise<void>;
  previewVoice: (voiceId: string, text: string) => Promise<void>;
  testDac: (text: string, voiceId?: string) => Promise<void>;
  testPage: (text: string, options?: { voiceId?: string; includeChime?: boolean }) => Promise<void>;
  uploadChime: (file: File) => Promise<void>;
  previewChime: (chimeId: string) => Promise<void>;
  setActiveChime: (chimeId: string) => Promise<void>;
  removeChime: (chimeId: string) => Promise<void>;
}

/**
 * Reads an error message from API envelopes and direct payload errors.
 * @param payload - Parsed JSON body.
 * @returns Error text when present.
 */
function readErrorMessage(payload: unknown): string | null {
  if (!payload || typeof payload !== "object") {
    return null;
  }
  if ("error" in payload) {
    const errorValue = (payload as { error?: unknown }).error;
    if (typeof errorValue === "string" && errorValue.trim().length > 0) {
      return errorValue;
    }
    if (errorValue && typeof errorValue === "object" && "message" in errorValue) {
      const message = (errorValue as { message?: unknown }).message;
      if (typeof message === "string" && message.trim().length > 0) {
        return message;
      }
    }
  }
  return null;
}

/**
 * Extracts endpoint data from either HomePi envelope or direct JSON payload.
 * @param payload - Parsed JSON response body.
 * @returns Typed payload body.
 */
function unpackResponseData<T>(payload: unknown): T {
  if (!payload || typeof payload !== "object") {
    return payload as T;
  }

  if ("ok" in payload) {
    const okFlag = (payload as { ok?: unknown }).ok;
    if (okFlag === false) {
      throw new Error(readErrorMessage(payload) ?? "Request failed");
    }
    if ("data" in payload) {
      const data = (payload as ApiResponse<T>).data;
      if (data !== undefined) {
        return data;
      }
    }
  }

  return payload as T;
}

/**
 * Performs a JSON request against backend paging endpoints.
 * @param path - Relative backend path.
 * @param init - Fetch options.
 * @returns Parsed payload data.
 */
async function requestJson<T>(path: string, init?: RequestInit): Promise<T> {
  const { apiBaseUrl } = getAppConfig();
  const response = await fetch(`${apiBaseUrl}${path}`, init);
  const payload = (await response.json()) as unknown;
  if (!response.ok) {
    throw new Error(readErrorMessage(payload) ?? `Request failed: ${response.status}`);
  }
  return unpackResponseData<T>(payload);
}

/**
 * Normalizes a chime list payload into an array.
 * @param payload - Returned chime payload.
 * @returns Chime rows.
 */
function normalizeChimes(payload: unknown): PagingChime[] {
  if (Array.isArray(payload)) {
    return payload as PagingChime[];
  }
  if (payload && typeof payload === "object" && "chimes" in payload) {
    const rows = (payload as { chimes?: unknown }).chimes;
    if (Array.isArray(rows)) {
      return rows as PagingChime[];
    }
  }
  return [];
}

/**
 * Normalizes a voice catalog payload into an array.
 * @param payload - Returned voice payload.
 * @returns Voice rows.
 */
function normalizeVoices(payload: unknown): PagingVoice[] {
  if (Array.isArray(payload)) {
    return payload as PagingVoice[];
  }
  if (payload && typeof payload === "object" && "voices" in payload) {
    const rows = (payload as { voices?: unknown }).voices;
    if (Array.isArray(rows)) {
      return rows as PagingVoice[];
    }
  }
  return [];
}

/**
 * Encodes a WAV file as base64 for chime upload requests.
 * @param file - Selected WAV file.
 * @returns Base64 payload without data URL prefix.
 */
async function fileToBase64(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => {
      if (typeof reader.result !== "string") {
        reject(new Error("Failed to read WAV file"));
        return;
      }
      const commaIndex = reader.result.indexOf(",");
      if (commaIndex < 0) {
        reject(new Error("Failed to encode WAV file"));
        return;
      }
      resolve(reader.result.slice(commaIndex + 1));
    };
    reader.onerror = () => {
      reject(reader.error ?? new Error("Failed to read WAV file"));
    };
    reader.readAsDataURL(file);
  });
}

/**
 * Derives a display name from an uploaded chime filename.
 * @param file - Uploaded WAV file.
 * @returns Human-readable chime name.
 */
function chimeDisplayNameFromFile(file: File): string {
  const baseName = file.name.replace(/\.[^.]+$/, "").trim();
  return baseName.length > 0 ? baseName : "Custom chime";
}

/**
 * Loads paging config, status, keys, voice catalog, and chimes for Audio UI.
 * @returns Paging state and operation handlers.
 */
export function usePagingModule(): UsePagingModuleResult {
  const [state, setState] = useState<PagingModuleState>({
    config: null,
    status: null,
    apiKeyInfo: null,
    voices: [],
    chimes: [],
    loadingConfig: true,
    loadingStatus: true,
    loadingApiKeyInfo: true,
    loadingVoices: true,
    loadingChimes: true,
    pendingActions: {},
    error: null,
  });

  /**
   * Tracks in-flight action state by action key.
   * @param actionKey - Stable operation key.
   * @param running - Whether the operation is active.
   */
  const setPending = useCallback((actionKey: string, running: boolean): void => {
    setState((current) => ({
      ...current,
      pendingActions: { ...current.pendingActions, [actionKey]: running },
    }));
  }, []);

  /**
   * Wraps an async action with pending state and error plumbing.
   * @param actionKey - Operation key.
   * @param task - Async operation function.
   * @returns Task result.
   */
  const runAction = useCallback(
    async <T>(actionKey: string, task: () => Promise<T>): Promise<T> => {
      setPending(actionKey, true);
      setState((current) => ({ ...current, error: null }));
      try {
        return await task();
      } catch (error) {
        setState((current) => ({
          ...current,
          error: error instanceof Error ? error.message : "Paging request failed",
        }));
        throw error;
      } finally {
        setPending(actionKey, false);
      }
    },
    [setPending]
  );

  /**
   * Reloads current paging config.
   */
  const refreshConfig = useCallback(async (): Promise<void> => {
    setState((current) => ({ ...current, loadingConfig: true }));
    try {
      const config = await requestJson<PagingConfig>("/api/audio/paging/config");
      setState((current) => ({ ...current, config, loadingConfig: false }));
    } catch (error) {
      setState((current) => ({
        ...current,
        loadingConfig: false,
        error: error instanceof Error ? error.message : "Failed to load paging config",
      }));
    }
  }, []);

  /**
   * Reloads current paging runtime status.
   */
  const refreshStatus = useCallback(async (): Promise<void> => {
    setState((current) => ({ ...current, loadingStatus: true }));
    try {
      const status = await requestJson<PagingStatus>("/api/audio/paging/status");
      setState((current) => ({ ...current, status, loadingStatus: false }));
    } catch (error) {
      setState((current) => ({
        ...current,
        loadingStatus: false,
        error: error instanceof Error ? error.message : "Failed to load paging status",
      }));
    }
  }, []);

  /**
   * Reloads paging API key summary shown in settings card.
   */
  const refreshApiKeyInfo = useCallback(async (): Promise<void> => {
    setState((current) => ({ ...current, loadingApiKeyInfo: true }));
    try {
      const settings = await requestJson<PagingApiKeyInfo>("/api/audio/settings");
      setState((current) => ({ ...current, apiKeyInfo: settings, loadingApiKeyInfo: false }));
    } catch (error) {
      setState((current) => ({
        ...current,
        loadingApiKeyInfo: false,
        error: error instanceof Error ? error.message : "Failed to load paging API key settings",
      }));
    }
  }, []);

  /**
   * Reloads paging voice catalog.
   */
  const refreshVoices = useCallback(async (): Promise<void> => {
    setState((current) => ({ ...current, loadingVoices: true }));
    try {
      const payload = await requestJson<unknown>("/api/audio/paging/voices/catalog");
      setState((current) => ({
        ...current,
        voices: normalizeVoices(payload),
        loadingVoices: false,
      }));
    } catch (error) {
      setState((current) => ({
        ...current,
        loadingVoices: false,
        error: error instanceof Error ? error.message : "Failed to load voice catalog",
      }));
    }
  }, []);

  /**
   * Reloads available paging chimes.
   */
  const refreshChimes = useCallback(async (): Promise<void> => {
    setState((current) => ({ ...current, loadingChimes: true }));
    try {
      const payload = await requestJson<unknown>("/api/audio/paging/chimes");
      setState((current) => ({
        ...current,
        chimes: normalizeChimes(payload),
        loadingChimes: false,
      }));
    } catch (error) {
      setState((current) => ({
        ...current,
        loadingChimes: false,
        error: error instanceof Error ? error.message : "Failed to load chimes",
      }));
    }
  }, []);

  /**
   * Refreshes all paging resources in parallel.
   */
  const refreshAll = useCallback(async (): Promise<void> => {
    await Promise.all([
      refreshConfig(),
      refreshStatus(),
      refreshApiKeyInfo(),
      refreshVoices(),
      refreshChimes(),
    ]);
  }, [refreshApiKeyInfo, refreshChimes, refreshConfig, refreshStatus, refreshVoices]);

  useEffect(() => {
    void refreshAll();
  }, [refreshAll]);

  /**
   * Updates persisted paging config and refreshes status.
   * @param patch - Config fields to update.
   */
  const updateConfig = useCallback(
    async (patch: PagingConfigPatch): Promise<void> => {
      await runAction("updateConfig", async () => {
        await requestJson<unknown>("/api/audio/paging/config", {
          method: "PUT",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(patch),
        });
        await Promise.all([refreshConfig(), refreshStatus(), refreshVoices()]);
      });
    },
    [refreshConfig, refreshStatus, refreshVoices, runAction]
  );

  /**
   * Enables or disables paging globally.
   * @param enabled - Desired paging enabled state.
   */
  const setEnabled = useCallback(
    async (enabled: boolean): Promise<void> => {
      await updateConfig({ enabled });
    },
    [updateConfig]
  );

  /**
   * Sets worker idle policy and keeps existing timeout value.
   * @param policy - Selected idle policy.
   */
  const setIdlePolicy = useCallback(
    async (policy: PagingConfigPatch["idlePolicy"]): Promise<void> => {
      if (!policy) {
        return;
      }
      const currentTimeout = state.config?.idleWarmTimeoutMs ?? 1_800_000;
      await updateConfig({ idlePolicy: policy, idleWarmTimeoutMs: currentTimeout });
    },
    [state.config?.idleWarmTimeoutMs, updateConfig]
  );

  /**
   * Sets default voice by updating paging config.
   * @param voiceId - Voice identifier.
   */
  const setDefaultVoice = useCallback(
    async (voiceId: string): Promise<void> => {
      await updateConfig({ defaultVoiceId: voiceId });
    },
    [updateConfig]
  );

  /**
   * Saves a user-chosen paging API key and refreshes summary info.
   * @param apiKey - Raw API key entered by the user.
   */
  const setApiKey = useCallback(
    async (apiKey: string): Promise<PagingApiKeySetResult> => {
      return runAction("setApiKey", async () => {
        const payload = await requestJson<PagingApiKeySetResult>(
          "/api/audio/settings/paging-api-key",
          {
            method: "PUT",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ apiKey }),
          }
        );
        await refreshApiKeyInfo();
        return payload;
      });
    },
    [refreshApiKeyInfo, runAction]
  );

  /**
   * Clears the configured paging API key.
   */
  const clearApiKey = useCallback(async (): Promise<void> => {
    await runAction("clearApiKey", async () => {
      await requestJson<unknown>("/api/audio/settings/paging-api-key", {
        method: "DELETE",
      });
      await refreshApiKeyInfo();
    });
  }, [refreshApiKeyInfo, runAction]);

  /**
   * Installs a voice and optionally sets it as default.
   * @param voiceId - Voice identifier.
   * @param setDefault - Whether to set as default after install.
   */
  const installVoice = useCallback(
    async (voiceId: string, setDefault: boolean): Promise<void> => {
      await runAction("installVoice", async () => {
        await requestJson<unknown>("/api/audio/paging/voices/install", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ voiceId, setDefault }),
        });
        await Promise.all([refreshVoices(), refreshConfig(), refreshStatus()]);
      });
    },
    [refreshConfig, refreshStatus, refreshVoices, runAction]
  );

  /**
   * Removes an installed voice.
   * @param voiceId - Voice identifier.
   */
  const removeVoice = useCallback(
    async (voiceId: string): Promise<void> => {
      await runAction("removeVoice", async () => {
        await requestJson<unknown>(`/api/audio/paging/voices/${encodeURIComponent(voiceId)}`, {
          method: "DELETE",
        });
        await Promise.all([refreshVoices(), refreshConfig(), refreshStatus()]);
      });
    },
    [refreshConfig, refreshStatus, refreshVoices, runAction]
  );

  /**
   * Requests a DAC-only voice preview clip.
   * @param voiceId - Voice to preview.
   * @param text - Preview text.
   */
  const previewVoice = useCallback(
    async (voiceId: string, text: string): Promise<void> => {
      await runAction("previewVoice", async () => {
        await requestJson<unknown>("/api/audio/paging/voices/preview", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ voiceId, text, output: "paging_dac_only" }),
        });
        await refreshStatus();
      });
    },
    [refreshStatus, runAction]
  );

  const testDac = useCallback(
    async (text: string, voiceId?: string): Promise<void> => {
      const resolvedVoiceId =
        voiceId ??
        state.config?.activeVoiceId ??
        state.config?.defaultVoiceId ??
        state.voices.find((voice) => voice.installed)?.voiceId;
      if (!resolvedVoiceId) {
        throw new Error("Install a voice before testing paging audio.");
      }
      await previewVoice(resolvedVoiceId, text);
    },
    [previewVoice, state.config?.activeVoiceId, state.config?.defaultVoiceId, state.voices]
  );

  /**
   * Runs a whole-house paging test using the preview-page flow.
   * @param text - Message to speak through the paging system.
   * @param options - Optional voice override and chime flag.
   */
  const testPage = useCallback(
    async (
      text: string,
      options?: { voiceId?: string; includeChime?: boolean }
    ): Promise<void> => {
      await runAction("testPage", async () => {
        await requestJson<unknown>("/api/audio/paging/preview-page", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            text,
            voiceId: options?.voiceId ?? null,
            includeChime: options?.includeChime ?? false,
          }),
        });
        await refreshStatus();
      });
    },
    [refreshStatus, runAction]
  );

  /**
   * Uploads a chime WAV file and refreshes chime list.
   * @param file - WAV file.
   */
  const uploadChime = useCallback(
    async (file: File): Promise<void> => {
      await runAction("uploadChime", async () => {
        const wavBase64 = await fileToBase64(file);
        await requestJson<unknown>("/api/audio/paging/chimes/upload", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            displayName: chimeDisplayNameFromFile(file),
            wavBase64,
          }),
        });
        await Promise.all([refreshChimes(), refreshConfig()]);
      });
    },
    [refreshChimes, refreshConfig, runAction]
  );

  /**
   * Requests a whole-house chime preview (HiFi PAGE + chime on paging DAC).
   * @param chimeId - Chime identifier.
   */
  const previewChime = useCallback(
    async (chimeId: string): Promise<void> => {
      await runAction("previewChime", async () => {
        await requestJson<unknown>("/api/audio/paging/chimes/preview", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ chimeId }),
        });
        await refreshStatus();
      });
    },
    [refreshStatus, runAction]
  );

  /**
   * Marks one chime as active.
   * @param chimeId - Chime identifier.
   */
  const setActiveChime = useCallback(
    async (chimeId: string): Promise<void> => {
      await runAction("setActiveChime", async () => {
        await requestJson<unknown>(`/api/audio/paging/chimes/${encodeURIComponent(chimeId)}/active`, {
          method: "PUT",
        });
        await Promise.all([refreshChimes(), refreshConfig()]);
      });
    },
    [refreshChimes, refreshConfig, runAction]
  );

  /**
   * Deletes a custom chime and refreshes active state.
   * @param chimeId - Chime identifier.
   */
  const removeChime = useCallback(
    async (chimeId: string): Promise<void> => {
      await runAction("removeChime", async () => {
        await requestJson<unknown>(`/api/audio/paging/chimes/${encodeURIComponent(chimeId)}`, {
          method: "DELETE",
        });
        await Promise.all([refreshChimes(), refreshConfig()]);
      });
    },
    [refreshChimes, refreshConfig, runAction]
  );

  return useMemo(
    () => ({
      state,
      refreshAll,
      refreshConfig,
      refreshStatus,
      refreshApiKeyInfo,
      refreshVoices,
      refreshChimes,
      updateConfig,
      setEnabled,
      setIdlePolicy,
      setDefaultVoice,
      setApiKey,
      clearApiKey,
      installVoice,
      removeVoice,
      previewVoice,
      testDac,
      testPage,
      uploadChime,
      previewChime,
      setActiveChime,
      removeChime,
    }),
    [
      installVoice,
      previewChime,
      previewVoice,
      testDac,
      testPage,
      refreshAll,
      refreshApiKeyInfo,
      refreshChimes,
      refreshConfig,
      refreshStatus,
      refreshVoices,
      setApiKey,
      clearApiKey,
      removeChime,
      removeVoice,
      setActiveChime,
      setDefaultVoice,
      setEnabled,
      setIdlePolicy,
      state,
      updateConfig,
      uploadChime,
    ]
  );
}
