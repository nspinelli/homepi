import { useCallback, useEffect, useRef, useState } from "react";

import { getAppConfig } from "@/config/app-config.js";
import { isZoneEnabled } from "@/lib/is-zone-enabled.js";
import { zoneInitialVolume } from "@/lib/zone-initial-volume.js";
import { showToast } from "@/lib/toast.js";
import type { EventEnvelope } from "@/types/dashboard-types.js";
import type {
  AudioApiResponse,
  AudioSnapshot,
  HifiController,
  HifiGroup,
  HifiSource,
  HifiZone,
  PcmMetadata,
  ShairportZoneSettings,
  ZoneSettingsPatch,
} from "@/types/audio-types.js";

/**
 * Live audio module state for dashboard and detail pages.
 */
export interface AudioModuleState {
  snapshot: AudioSnapshot | null;
  loading: boolean;
  error: string | null;
  savingZone: number | null;
  togglingPowerZone: number | null;
  sseConnected: boolean;
}

const initialSnapshot: AudioSnapshot = {
  controller: {},
  zones: [],
  sources: [],
  groups: [],
  shairportZoneSettings: [],
  pcm: { ownerZoneId: 0, activeStack: [], dacState: "unknown", metadata: {} },
  services: {
    hifiSerial: "offline",
    shairport: "offline",
    pcmRouter: "offline",
    nqptp: "offline",
    metadata: "offline",
  },
  hifiConnected: false,
};

/**
 * Moves a zone to the front of the PCM active stack (route_start semantics).
 * @param stack - Current stack.
 * @param zoneId - Zone to prioritize.
 * @returns Updated stack.
 */
function pushPcmStack(stack: number[], zoneId: number): number[] {
  return [zoneId, ...stack.filter((zone) => zone !== zoneId)];
}

/**
 * Appends a zone to the PCM active stack when joining an existing session.
 * @param stack - Current stack.
 * @param zoneId - Zone joining the stack.
 * @returns Updated stack.
 */
function joinPcmStack(stack: number[], zoneId: number): number[] {
  if (stack.includes(zoneId)) {
    return stack;
  }
  return [...stack, zoneId];
}

/**
 * Removes a zone from the PCM active stack.
 * @param stack - Current stack.
 * @param zoneId - Zone leaving the stack.
 * @returns Updated stack.
 */
function removePcmStack(stack: number[], zoneId: number): number[] {
  return stack.filter((zone) => zone !== zoneId);
}

/**
 * Removes a zone from live PCM routing and clears now-playing when the stack empties.
 * @param pcm - Current PCM state.
 * @param zoneNumber - Zone leaving the route.
 * @returns Updated PCM state.
 */
function clearZoneFromPcmRouting(
  pcm: AudioSnapshot["pcm"],
  zoneNumber: number
): AudioSnapshot["pcm"] {
  const activeStack = removePcmStack(pcm.activeStack, zoneNumber);
  const ownerZoneId = activeStack[0] ?? 0;
  return {
    ...pcm,
    activeStack,
    ownerZoneId,
    metadata: ownerZoneId === 0 ? {} : pcm.metadata,
  };
}

/**
 * Returns the zone currently feeding the DAC (stack head).
 * @param pcm - PCM routing state.
 * @returns Owner zone id or 0.
 */
function getDacOwnerZoneId(pcm: AudioSnapshot["pcm"]): number {
  if (pcm.activeStack.length > 0) {
    return pcm.activeStack[0] ?? 0;
  }
  return pcm.ownerZoneId;
}

/**
 * Sort priority for zone cards: DAC owner, streaming, on, off.
 * Lower values appear first.
 * @param zone - Hi-Fi zone row.
 * @param isSendingAudio - Whether the zone feeds the DAC.
 * @param isStreamedTo - Whether the zone is in the PCM active stack.
 * @returns Priority rank 0-4.
 */
export function getZoneActivityPriority(
  zone: HifiZone,
  isSendingAudio: boolean,
  isStreamedTo: boolean
): number {
  if (!isZoneEnabled(zone)) {
    return 4;
  }
  if (isSendingAudio) {
    return 0;
  }
  if (isStreamedTo) {
    return 1;
  }
  if ((zone.power ?? 0) === 1) {
    return 2;
  }
  return 3;
}

/**
 * Applies a PCM router envelope to snapshot routing state.
 * @param pcm - Current PCM state.
 * @param event - Event name.
 * @param payload - Event payload.
 * @returns Updated PCM state.
 */
function patchPcmFromEvent(
  pcm: AudioSnapshot["pcm"],
  event: string,
  payload: Record<string, unknown>
): AudioSnapshot["pcm"] {
  let activeStack = [...pcm.activeStack];
  let ownerZoneId = pcm.ownerZoneId;
  let dacState = pcm.dacState;

  if (event === "pcm_router_snapshot") {
    if (Array.isArray(payload.activeStack)) {
      activeStack = payload.activeStack as number[];
    }
    if (typeof payload.ownerZoneId === "number") {
      ownerZoneId = payload.ownerZoneId;
    }
    if (typeof payload.dacState === "string") {
      dacState = payload.dacState;
    }
    return { ...pcm, ownerZoneId, activeStack, dacState };
  }

  if (event === "owner_cleared") {
    return { ...pcm, ownerZoneId: 0, activeStack: [], metadata: {} };
  }

  if (event === "owner_changed") {
    if (typeof payload.ownerZoneId === "number") {
      ownerZoneId = payload.ownerZoneId;
      if (ownerZoneId === 0) {
        activeStack = [];
      } else {
        activeStack = pushPcmStack(activeStack, ownerZoneId);
      }
    }
    if (Array.isArray(payload.activeStack)) {
      activeStack = payload.activeStack as number[];
    }
    return { ...pcm, ownerZoneId, activeStack };
  }

  if (event === "zone_updated") {
    const zoneId =
      typeof payload.zoneId === "number"
        ? payload.zoneId
        : typeof payload.zone === "number"
          ? payload.zone
          : null;
    if (zoneId === null) {
      return pcm;
    }

    const joined = payload.joined === true;
    const active = payload.active !== false;

    if (active && joined) {
      activeStack = joinPcmStack(activeStack, zoneId);
    } else if (active) {
      activeStack = pushPcmStack(activeStack, zoneId);
    } else {
      activeStack = removePcmStack(activeStack, zoneId);
    }

    ownerZoneId = activeStack[0] ?? 0;
    return { ...pcm, ownerZoneId, activeStack };
  }

  return pcm;
}

/**
 * Parses JSON from an audio API response envelope.
 * @param response - Fetch response.
 * @returns Parsed data.
 */
async function fetchAudioJson<T>(url: string): Promise<T> {
  const response = await fetch(url);
  const body = (await response.json()) as AudioApiResponse<T>;
  if (!body.ok || body.data === undefined) {
    throw new Error(body.error?.message ?? `Request failed: ${url}`);
  }
  return body.data;
}

/**
 * Applies a Hi-Fi zone delta event to zone list state.
 * @param zones - Current zones.
 * @param event - Event name.
 * @param payload - Event payload.
 * @returns Updated zones.
 */
function patchZonesFromEvent(
  zones: HifiZone[],
  event: string,
  payload: Record<string, unknown>
): HifiZone[] {
  const zoneNum =
    typeof payload.zone === "number"
      ? payload.zone
      : typeof payload.zoneId === "number"
        ? payload.zoneId
        : null;
  if (zoneNum === null) {
    return zones;
  }
  return zones.map((zone) => {
    if (zone.zoneNumber !== zoneNum) {
      return zone;
    }
    const next = { ...zone };
    if (event === "zone_name_changed" && typeof payload.name === "string") {
      next.name = payload.name;
    }
    if (event === "zone_enable_changed" && typeof payload.enabled === "number") {
      next.enabled = payload.enabled;
    }
    if (event === "zone_power_changed" && typeof payload.power === "number") {
      next.power = payload.power;
    }
    if (event === "zone_volume_changed" && typeof payload.volume === "number") {
      next.volume = payload.volume;
    }
    if (event === "zone_treble_changed" && typeof payload.treble === "number") {
      next.treble = payload.treble;
    }
    if (event === "zone_bass_changed" && typeof payload.bass === "number") {
      next.bass = payload.bass;
    }
    if (event === "zone_balance_changed" && typeof payload.balance === "number") {
      next.balance = payload.balance;
    }
    if (event === "zone_loudness_changed" && typeof payload.loudness === "number") {
      next.loudness = payload.loudness;
    }
    if (
      event === "zone_initial_volume_changed" &&
      typeof payload.initialVolume === "number"
    ) {
      next.initialVolume = payload.initialVolume;
    }
    if (event === "zone_page_volume_changed" && typeof payload.pageVolume === "number") {
      next.pageVolume = payload.pageVolume;
    }
    if (event === "zone_group_changed" && typeof payload.groupNumber === "number") {
      next.groupNumber = payload.groupNumber;
    }
    if (event === "zone_source_changed" && typeof payload.source === "number") {
      next.source = payload.source;
    }
    return next;
  });
}

/**
 * Loads audio snapshot and maintains live SSE updates.
 * @returns Audio module state and actions.
 */
export function useAudioModule(): {
  state: AudioModuleState;
  refresh: () => Promise<void>;
  saveZoneSettings: (zoneNumber: number, patch: ZoneSettingsPatch) => Promise<void>;
  toggleZonePower: (zoneNumber: number) => Promise<void>;
  setZoneVolume: (zoneNumber: number, volume: number) => Promise<void>;
  isZoneStreamedTo: (zoneNumber: number) => boolean;
  isZoneSendingAudio: (zoneNumber: number) => boolean;
  nowPlaying: { track?: string; artist?: string; album?: string; source?: string } | null;
} {
  const [state, setState] = useState<AudioModuleState>({
    snapshot: null,
    loading: true,
    error: null,
    savingZone: null,
    togglingPowerZone: null,
    sseConnected: false,
  });
  const eventSourceRef = useRef<EventSource | null>(null);

  const applyEnvelope = useCallback((envelope: EventEnvelope) => {
    setState((current) => {
      if (!current.snapshot) {
        return current;
      }
      const snapshot = { ...current.snapshot };

      if (envelope.source === "homepi-hifi-serial") {
        if (envelope.event === "audio_state_snapshot") {
          const payload = envelope.payload as {
            controller?: HifiController;
            zones?: HifiZone[];
            sources?: HifiSource[];
            groups?: HifiGroup[];
          };
          if (payload.controller) {
            snapshot.controller = payload.controller;
          }
          if (payload.zones) {
            snapshot.zones = payload.zones;
          }
          if (payload.sources) {
            snapshot.sources = payload.sources;
          }
          if (payload.groups) {
            snapshot.groups = payload.groups;
          }
        } else if (envelope.event.startsWith("zone_")) {
          snapshot.zones = patchZonesFromEvent(
            snapshot.zones,
            envelope.event,
            envelope.payload as Record<string, unknown>
          );
        }
      }

      if (envelope.source === "homepi-pcm-router") {
        const payload = envelope.payload as Record<string, unknown>;
        snapshot.pcm = patchPcmFromEvent(snapshot.pcm, envelope.event, payload);

        const routedZoneId =
          typeof payload.zoneId === "number"
            ? payload.zoneId
            : typeof payload.zone === "number"
              ? payload.zone
              : null;

        if (
          envelope.event === "zone_volume_changed" &&
          typeof payload.volume === "number" &&
          routedZoneId !== null
        ) {
          snapshot.zones = patchZonesFromEvent(snapshot.zones, envelope.event, {
            zone: routedZoneId,
            volume: payload.volume,
          });
        }

        if (envelope.event === "zone_updated" && routedZoneId !== null) {
          if (payload.active === true) {
            snapshot.zones = snapshot.zones.map((zone) =>
              zone.zoneNumber === routedZoneId && (zone.power ?? 0) === 0
                ? { ...zone, power: 1 }
                : zone
            );
          } else {
            snapshot.zones = snapshot.zones.map((zone) =>
              zone.zoneNumber === routedZoneId
                ? {
                    ...zone,
                    power: 0,
                    volume: zoneInitialVolume(zone),
                  }
                : zone
            );
          }
        }

        if (
          snapshot.pcm.ownerZoneId === 0 &&
          snapshot.pcm.activeStack.length === 0
        ) {
          snapshot.pcm = { ...snapshot.pcm, metadata: {} };
        }

        const metaField =
          typeof payload.field === "string"
            ? payload.field
            : typeof payload.metadataField === "string"
              ? payload.metadataField
              : null;
        const metaValue = typeof payload.value === "string" ? payload.value : null;
        if (metaField && metaValue !== null) {
          const metadata: PcmMetadata = { ...snapshot.pcm.metadata };
          if (metaField === "title") {
            metadata.title = metaValue;
          }
          if (metaField === "artist") {
            metadata.artist = metaValue;
          }
          if (metaField === "album") {
            metadata.album = metaValue;
          }
          if (metaField === "client_name" || metaField === "clientName") {
            metadata.clientName = metaValue;
          }
          snapshot.pcm = { ...snapshot.pcm, metadata };
        }
      }

      return { ...current, snapshot };
    });
  }, []);

  const refresh = useCallback(async () => {
    const config = getAppConfig();
    setState((current) => ({ ...current, loading: true, error: null }));
    try {
      const data = await fetchAudioJson<AudioSnapshot>(
        `${config.apiBaseUrl}/api/audio/snapshot`
      );
      setState((current) => ({
        ...current,
        snapshot: data,
        loading: false,
        error: null,
      }));
    } catch (error) {
      setState((current) => ({
        ...current,
        snapshot: current.snapshot ?? initialSnapshot,
        loading: false,
        error: error instanceof Error ? error.message : "Failed to load audio state",
      }));
    }
  }, []);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  useEffect(() => {
    const config = getAppConfig();
    const source = new EventSource(config.eventsUrl);
    eventSourceRef.current = source;

    source.onopen = () => {
      setState((current) => ({ ...current, sseConnected: true }));
    };

    source.onerror = () => {
      setState((current) => ({ ...current, sseConnected: false }));
    };

    source.addEventListener("envelope", (event) => {
      try {
        const envelope = JSON.parse(event.data) as EventEnvelope;
        if (
          envelope.source === "homepi-hifi-serial" ||
          envelope.source === "homepi-pcm-router"
        ) {
          applyEnvelope(envelope);
        }
      } catch {
        /* ignore malformed events */
      }
    });

    return () => {
      source.close();
      eventSourceRef.current = null;
    };
  }, [applyEnvelope]);

  const saveZoneSettings = useCallback(
    async (zoneNumber: number, patch: ZoneSettingsPatch): Promise<void> => {
      const config = getAppConfig();
      setState((current) => ({
        ...current,
        savingZone: zoneNumber,
      }));
      try {
        const result = await fetch(`${config.apiBaseUrl}/api/audio/zones/${zoneNumber}`, {
          method: "PUT",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(patch),
        }).then(async (response) => {
          const body = (await response.json()) as AudioApiResponse<{
            shairportRestartRequired: boolean;
          }>;
          if (!body.ok) {
            throw new Error(body.error?.message ?? "Save failed");
          }
          return body.data;
        });

        setState((current) => {
          if (!current.snapshot) {
            return { ...current, savingZone: null };
          }

          const zones = current.snapshot.zones.map((row) => {
            if (row.zoneNumber !== zoneNumber) {
              return row;
            }
            return { ...row, ...(patch.controller ?? {}) };
          });

          let shairportZoneSettings = current.snapshot.shairportZoneSettings;
          if (patch.shairport && Object.keys(patch.shairport).length > 0) {
            shairportZoneSettings = shairportZoneSettings.map((row) =>
              row.zoneNumber === zoneNumber ? { ...row, ...patch.shairport } : row
            );
          }

          return {
            ...current,
            savingZone: null,
            snapshot: {
              ...current.snapshot,
              zones,
              shairportZoneSettings,
            },
          };
        });

        showToast(
          result?.shairportRestartRequired
            ? `Zone ${zoneNumber} AirPlay settings saved; zone will restart when the supervisor applies changes.`
            : `Zone ${zoneNumber} settings saved.`,
          "success"
        );
      } catch (error) {
        setState((current) => ({
          ...current,
          savingZone: null,
        }));
        showToast(error instanceof Error ? error.message : "Save failed", "error");
        throw error;
      }
    },
    []
  );

  const toggleZonePower = useCallback(
    async (zoneNumber: number) => {
      const config = getAppConfig();
      const zone = state.snapshot?.zones.find((row) => row.zoneNumber === zoneNumber);
      if (!zone || !isZoneEnabled(zone)) {
        return;
      }

      const nextPower = (zone.power ?? 0) === 1 ? 0 : 1;
      const initialVolume = zoneInitialVolume(zone);
      const controllerPatch =
        nextPower === 0
          ? { power: nextPower, volume: initialVolume }
          : { power: nextPower };

      setState((current) => {
        if (!current.snapshot) {
          return { ...current, togglingPowerZone: zoneNumber };
        }
        const nextPcm =
          nextPower === 0
            ? clearZoneFromPcmRouting(current.snapshot.pcm, zoneNumber)
            : current.snapshot.pcm;

        return {
          ...current,
          togglingPowerZone: zoneNumber,
          snapshot: {
            ...current.snapshot,
            pcm: nextPcm,
            zones: current.snapshot.zones.map((row) =>
              row.zoneNumber === zoneNumber
                ? {
                    ...row,
                    power: nextPower,
                    volume: nextPower === 0 ? initialVolume : row.volume,
                  }
                : row
            ),
          },
        };
      });

      try {
        const response = await fetch(`${config.apiBaseUrl}/api/audio/zones/${zoneNumber}`, {
          method: "PUT",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ controller: controllerPatch }),
        });
        const body = (await response.json()) as AudioApiResponse<{ zoneNumber: number }>;
        if (!body.ok) {
          throw new Error(body.error?.message ?? "Power toggle failed");
        }
      } catch (error) {
        showToast(error instanceof Error ? error.message : "Power toggle failed", "error");
      } finally {
        setState((current) => ({
          ...current,
          togglingPowerZone: null,
        }));
      }
    },
    [state.snapshot?.zones]
  );

  const setZoneVolume = useCallback(async (zoneNumber: number, volume: number) => {
    const config = getAppConfig();
    const clamped = Math.max(0, Math.min(100, Math.round(volume)));

    setState((current) => {
      if (!current.snapshot) {
        return current;
      }
      return {
        ...current,
        snapshot: {
          ...current.snapshot,
          zones: current.snapshot.zones.map((zone) =>
            zone.zoneNumber === zoneNumber ? { ...zone, volume: clamped } : zone
          ),
        },
      };
    });

    try {
      const response = await fetch(`${config.apiBaseUrl}/api/audio/zones/${zoneNumber}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ controller: { volume: clamped } }),
      });
      const body = (await response.json()) as AudioApiResponse<{ zoneNumber: number }>;
      if (!body.ok) {
        throw new Error(body.error?.message ?? "Volume update failed");
      }
    } catch {
      /* SSE/controller echo is authoritative; ignore transient write failures */
    }
  }, []);

  const isZoneStreamedTo = useCallback(
    (zoneNumber: number): boolean => {
      const snapshot = state.snapshot;
      if (!snapshot) {
        return false;
      }
      const zone = snapshot.zones.find((row) => row.zoneNumber === zoneNumber);
      if (!zone || !isZoneEnabled(zone)) {
        return false;
      }
      return snapshot.pcm.activeStack.includes(zoneNumber);
    },
    [state.snapshot]
  );

  const isZoneSendingAudio = useCallback(
    (zoneNumber: number): boolean => {
      const snapshot = state.snapshot;
      if (!snapshot) {
        return false;
      }
      return getDacOwnerZoneId(snapshot.pcm) === zoneNumber;
    },
    [state.snapshot]
  );

  const nowPlaying = (() => {
    const snapshot = state.snapshot;
    if (!snapshot) {
      return null;
    }
    const ownerZoneId = getDacOwnerZoneId(snapshot.pcm);
    const { title, artist, album, clientName } = snapshot.pcm.metadata;
    if (ownerZoneId <= 0) {
      return null;
    }
    if (!title && !artist) {
      return null;
    }
    return {
      track: title,
      artist,
      album,
      source: clientName ?? `Zone ${ownerZoneId}`,
    };
  })();

  return {
    state,
    refresh,
    saveZoneSettings,
    toggleZonePower,
    setZoneVolume,
    isZoneStreamedTo,
    isZoneSendingAudio,
    nowPlaying,
  };
}

/**
 * Finds Shairport settings for a zone.
 * @param settings - All zone settings.
 * @param zoneNumber - Zone number.
 * @returns Matching settings row.
 */
export function getShairportSettingsForZone(
  settings: ShairportZoneSettings[],
  zoneNumber: number
): ShairportZoneSettings | undefined {
  return settings.find((row) => row.zoneNumber === zoneNumber);
}
