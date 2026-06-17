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
  AudioPlaybackView,
  PcmMetadata,
  PcmProfileTuple,
  PlaybackRemoteCommand,
  ShairportZoneSettings,
  SourceSettingsPatch,
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
  savingSource: number | null;
  togglingPowerZone: number | null;
  sseConnected: boolean;
}

const defaultPcmPlayback: AudioSnapshot["pcm"]["playback"] = {
  playing: false,
  positionMs: 0,
  durationMs: 0,
};

/**
 * Ensures PCM playback fields exist when loading snapshots from older backends.
 * @param snapshot - Raw audio snapshot from the API or SSE.
 * @returns Snapshot with normalized PCM playback state.
 */
function normalizeAudioSnapshot(snapshot: AudioSnapshot): AudioSnapshot {
  return {
    ...snapshot,
    pcm: {
      ...snapshot.pcm,
      playback: {
        ...defaultPcmPlayback,
        ...snapshot.pcm.playback,
      },
    },
  };
}

const initialSnapshot: AudioSnapshot = {
  controller: {},
  zones: [],
  sources: [],
  groups: [],
  shairportZoneSettings: [],
  pcm: {
    ownerZoneId: 0,
    activeStack: [],
    dacState: "unknown",
    metadata: {},
    playback: { playing: false, positionMs: 0, durationMs: 0 },
  },
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
    playback:
      ownerZoneId === 0
        ? { playing: false, positionMs: 0, durationMs: 0 }
        : pcm.playback,
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

function metadataTargetsOwnerZone(
  pcm: AudioSnapshot["pcm"],
  payload: Record<string, unknown>
): boolean {
  const payloadOwner =
    typeof payload.ownerZoneId === "number"
      ? payload.ownerZoneId
      : typeof payload.zoneId === "number"
        ? payload.zoneId
        : 0;
  const ownerZoneId = getDacOwnerZoneId(pcm);
  if (payloadOwner <= 0) {
    return false;
  }
  if (ownerZoneId <= 0) {
    return true;
  }
  return payloadOwner === ownerZoneId;
}

/**
 * Applies a metadata field update to PCM now-playing state.
 * @param pcm - Current PCM state.
 * @param payload - Metadata event payload.
 * @returns Updated PCM state.
 */
function applyMetadataFieldToPcm(
  pcm: AudioSnapshot["pcm"],
  payload: Record<string, unknown>
): AudioSnapshot["pcm"] {
  if (!metadataTargetsOwnerZone(pcm, payload)) {
    return pcm;
  }

  const metaField =
    typeof payload.field === "string"
      ? payload.field
      : typeof payload.metadataField === "string"
        ? payload.metadataField
        : null;
  const metaValue = typeof payload.value === "string" ? payload.value : null;
  if (!metaField || metaValue === null) {
    return pcm;
  }

  const metadata: PcmMetadata = { ...pcm.metadata };
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
  if (metaField === "client_model" || metaField === "clientModel") {
    metadata.clientModel = metaValue;
  }
  return { ...pcm, metadata };
}

function readMetadataField(
  payload: Record<string, unknown>,
  key: string,
  current: string | undefined
): string | undefined {
  const value = payload[key];
  if (typeof value !== "string") {
    return current;
  }
  return value.length > 0 ? value : current;
}

/**
 * Returns true when a metadata snapshot carries displayable track text.
 * @param payload - Metadata snapshot payload.
 * @returns Whether any text field is non-empty.
 */
function hasMetadataText(payload: Record<string, unknown>): boolean {
  return ["title", "artist", "album", "clientName"].some(
    (key) => typeof payload[key] === "string" && (payload[key] as string).length > 0
  );
}

/**
 * Applies a metadata snapshot payload to PCM now-playing state.
 * @param pcm - Current PCM state.
 * @param payload - Metadata snapshot payload.
 * @returns Updated PCM state.
 */
function applyMetadataSnapshotToPcm(
  pcm: AudioSnapshot["pcm"],
  payload: Record<string, unknown>
): AudioSnapshot["pcm"] {
  if (!metadataTargetsOwnerZone(pcm, payload)) {
    return pcm;
  }

  const hasText = hasMetadataText(payload);
  if (!hasText && payload.playing !== true && payload.hasCoverArt !== true) {
    return pcm;
  }

  return {
    ...pcm,
    metadata: {
      title: readMetadataField(payload, "title", pcm.metadata.title),
      artist: readMetadataField(payload, "artist", pcm.metadata.artist),
      album: readMetadataField(payload, "album", pcm.metadata.album),
      clientName: readMetadataField(payload, "clientName", pcm.metadata.clientName),
      clientModel: readMetadataField(payload, "clientModel", pcm.metadata.clientModel),
    },
    playback: {
      playing:
        payload.playing === true
          ? true
          : hasText && payload.playing === false
            ? false
            : pcm.playback.playing,
      positionMs:
        typeof payload.positionMs === "number" ? payload.positionMs : pcm.playback.positionMs,
      durationMs:
        typeof payload.durationMs === "number" ? payload.durationMs : pcm.playback.durationMs,
      progressSyncedAt: Date.now(),
    },
    hasCoverArt: payload.hasCoverArt === true ? true : pcm.hasCoverArt,
  };
}

/**
 * Applies metadata progress or playback state to PCM playback fields.
 * @param pcm - Current PCM state.
 * @param payload - Metadata progress payload.
 * @returns Updated PCM state.
 */
function applyMetadataProgressToPcm(
  pcm: AudioSnapshot["pcm"],
  payload: Record<string, unknown>
): AudioSnapshot["pcm"] {
  if (!metadataTargetsOwnerZone(pcm, payload)) {
    return pcm;
  }

  const playback = { ...pcm.playback };
  if (typeof payload.positionMs === "number") {
    playback.positionMs = payload.positionMs;
  }
  if (typeof payload.durationMs === "number") {
    playback.durationMs = payload.durationMs;
  }
  if (typeof payload.playing === "boolean") {
    playback.playing = payload.playing;
  }
  playback.progressSyncedAt = Date.now();
  return { ...pcm, playback };
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
  const normalizedPcm = {
    ...pcm,
    playback: pcm.playback ?? { ...defaultPcmPlayback },
  };
  let activeStack = [...normalizedPcm.activeStack];
  let ownerZoneId = normalizedPcm.ownerZoneId;
  let dacState = normalizedPcm.dacState;
  pcm = normalizedPcm;

  if (event === "routing_changed" || event === "pcm_router_snapshot") {
    if (Array.isArray(payload.activeStack)) {
      activeStack = payload.activeStack as number[];
    }
    if (typeof payload.ownerZoneId === "number") {
      ownerZoneId = payload.ownerZoneId;
    }
    if (
      event === "pcm_router_snapshot" &&
      ownerZoneId === 0 &&
      activeStack.length === 0 &&
      getDacOwnerZoneId(normalizedPcm) > 0
    ) {
      activeStack = [...normalizedPcm.activeStack];
      ownerZoneId = normalizedPcm.ownerZoneId;
    }
    if (typeof payload.dacState === "string") {
      dacState = payload.dacState;
    }
    const profileMode =
      typeof payload.profileMode === "string" ? payload.profileMode : pcm.profileMode;
    const profileStatus =
      typeof payload.profileStatus === "string" ? payload.profileStatus : pcm.profileStatus;
    const profileRevision =
      typeof payload.profileRevision === "number"
        ? payload.profileRevision
        : pcm.profileRevision;
    const profileSource =
      typeof payload.profileSource === "string" ? payload.profileSource : pcm.profileSource;
    const audioBridgeState =
      typeof payload.audioBridgeState === "string"
        ? payload.audioBridgeState
        : pcm.audioBridgeState;
    const parseTuple = (value: unknown): PcmProfileTuple | undefined => {
      if (typeof value !== "object" || value === null) {
        return undefined;
      }
      const tuple = value as Record<string, unknown>;
      if (
        typeof tuple.sampleRate !== "number" ||
        typeof tuple.channels !== "number" ||
        typeof tuple.sampleFormat !== "string"
      ) {
        return undefined;
      }
      return {
        sampleRate: tuple.sampleRate,
        channels: tuple.channels,
        sampleFormat: tuple.sampleFormat,
      };
    };
    const loopbackProfile = parseTuple(payload.loopbackProfile) ?? pcm.loopbackProfile;
    const dacProfile = parseTuple(payload.dacProfile) ?? pcm.dacProfile;
    return {
      ...pcm,
      ownerZoneId,
      activeStack,
      dacState,
      profileMode,
      profileStatus,
      loopbackProfile,
      dacProfile,
      profileRevision,
      profileSource,
      audioBridgeState,
    };
  }

  if (event === "pcm_cover_updated") {
    const zoneId =
      typeof payload.zoneId === "number"
        ? payload.zoneId
        : typeof payload.zone === "number"
          ? payload.zone
          : null;
    if (zoneId === null || zoneId !== getDacOwnerZoneId({ ...pcm, ownerZoneId, activeStack })) {
      return pcm;
    }
    return { ...pcm, hasCoverArt: true };
  }

  if (event === "playback_state_changed") {
    const zoneId =
      typeof payload.zoneId === "number"
        ? payload.zoneId
        : typeof payload.zone === "number"
          ? payload.zone
          : null;
    if (zoneId === null || zoneId !== getDacOwnerZoneId({ ...pcm, ownerZoneId, activeStack })) {
      return pcm;
    }
    const playing = payload.playing === true;
    return {
      ...pcm,
      playback: { ...pcm.playback, playing },
    };
  }

  if (event === "pcm_progress_updated") {
    const zoneId =
      typeof payload.zoneId === "number"
        ? payload.zoneId
        : typeof payload.zone === "number"
          ? payload.zone
          : null;
    if (zoneId === null || zoneId !== getDacOwnerZoneId({ ...pcm, ownerZoneId, activeStack })) {
      return pcm;
    }
    const playback = { ...pcm.playback };
    if (typeof payload.positionMs === "number") {
      playback.positionMs = payload.positionMs;
    }
    if (typeof payload.durationMs === "number") {
      playback.durationMs = payload.durationMs;
    }
    playback.progressSyncedAt = Date.now();
    return { ...pcm, playback };
  }

  if (event === "owner_cleared") {
    return {
      ...pcm,
      ownerZoneId: 0,
      activeStack: [],
      metadata: {},
      playback: { playing: false, positionMs: 0, durationMs: 0 },
    };
  }

  if (event === "owner_changed") {
    if (Array.isArray(payload.activeStack)) {
      activeStack = payload.activeStack as number[];
      ownerZoneId = activeStack[0] ?? 0;
    } else if (typeof payload.ownerZoneId === "number") {
      ownerZoneId = payload.ownerZoneId;
      if (ownerZoneId === 0) {
        activeStack = [];
      } else {
        activeStack = pushPcmStack(activeStack, ownerZoneId);
      }
    }
    return { ...pcm, ownerZoneId, activeStack };
  }

  if (event === "zone_updated") {
    return pcm;
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
 * Applies a Hi-Fi source delta event to source list state.
 * @param sources - Current sources.
 * @param event - Event name.
 * @param payload - Event payload.
 * @returns Updated sources.
 */
function patchSourcesFromEvent(
  sources: HifiSource[],
  event: string,
  payload: Record<string, unknown>
): HifiSource[] {
  if (event === "source_airplay_changed") {
    const sourceNumber =
      typeof payload.sourceNumber === "number" ? payload.sourceNumber : null;
    if (sourceNumber === null) {
      return sources;
    }
    return sources.map((source) => ({
      ...source,
      isAirplay: source.sourceNumber === sourceNumber ? 1 : 0,
    }));
  }

  const sourceNum = typeof payload.source === "number" ? payload.source : null;
  if (sourceNum === null) {
    return sources;
  }

  return sources.map((source) => {
    if (source.sourceNumber !== sourceNum) {
      return source;
    }
    const next = { ...source };
    if (event === "source_name_changed" && typeof payload.name === "string") {
      next.name = payload.name;
    }
    if (event === "source_enable_changed" && typeof payload.enabled === "number") {
      next.enabled = payload.enabled;
    }
    if (event === "source_input_gain_changed" && typeof payload.inputGain === "number") {
      next.inputGain = payload.inputGain;
    }
    if (event === "source_display_line_changed" && typeof payload.displayLine === "string") {
      next.displayLine = payload.displayLine;
    }
    return next;
  });
}

/**
 * Loads audio snapshot and maintains live SSE updates.
 * @returns Audio module state and actions.
 */
function useAudioModuleState(): {
  state: AudioModuleState;
  refresh: () => Promise<void>;
  saveZoneSettings: (zoneNumber: number, patch: ZoneSettingsPatch) => Promise<void>;
  saveSourceSettings: (sourceNumber: number, patch: SourceSettingsPatch) => Promise<void>;
  toggleZonePower: (zoneNumber: number) => Promise<void>;
  setZoneVolume: (zoneNumber: number, volume: number) => Promise<void>;
  isZoneStreamedTo: (zoneNumber: number) => boolean;
  isZoneSendingAudio: (zoneNumber: number) => boolean;
  nowPlaying: { track?: string; artist?: string; album?: string; source?: string } | null;
  playback: AudioPlaybackView | null;
  sendPlaybackCommand: (command: PlaybackRemoteCommand) => Promise<void>;
  setPlaybackVolume: (volume: number) => Promise<void>;
} {
  const [state, setState] = useState<AudioModuleState>({
    snapshot: null,
    loading: true,
    error: null,
    savingZone: null,
    savingSource: null,
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
        } else if (
          envelope.event.startsWith("source_") ||
          envelope.event === "source_airplay_changed"
        ) {
          snapshot.sources = patchSourcesFromEvent(
            snapshot.sources,
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

      }

      if (envelope.source === "homepi-metadata") {
        const payload = envelope.payload as Record<string, unknown>;
        if (envelope.event === "metadata_snapshot") {
          snapshot.pcm = applyMetadataSnapshotToPcm(snapshot.pcm, payload);
        } else if (envelope.event === "metadata_field_updated") {
          snapshot.pcm = applyMetadataFieldToPcm(snapshot.pcm, payload);
        } else if (
          envelope.event === "metadata_progress_updated" ||
          envelope.event === "playback_state_changed"
        ) {
          snapshot.pcm = applyMetadataProgressToPcm(snapshot.pcm, payload);
        } else if (envelope.event === "metadata_cover_updated") {
          const ownerZoneId = getDacOwnerZoneId(snapshot.pcm);
          const zoneId = typeof payload.zoneId === "number" ? payload.zoneId : ownerZoneId;
          if (zoneId === ownerZoneId && ownerZoneId > 0) {
            snapshot.pcm = { ...snapshot.pcm, hasCoverArt: true };
          }
        } else if (envelope.event === "metadata_cleared") {
          snapshot.pcm = {
            ...snapshot.pcm,
            metadata: {},
            playback: { playing: false, positionMs: 0, durationMs: 0 },
            hasCoverArt: false,
          };
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
        snapshot: normalizeAudioSnapshot(data),
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
          envelope.source === "homepi-pcm-router" ||
          envelope.source === "homepi-metadata"
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

  const saveSourceSettings = useCallback(
    async (sourceNumber: number, patch: SourceSettingsPatch): Promise<void> => {
      const config = getAppConfig();
      setState((current) => ({
        ...current,
        savingSource: sourceNumber,
      }));
      try {
        const result = await fetch(`${config.apiBaseUrl}/api/audio/sources/${sourceNumber}`, {
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
            return { ...current, savingSource: null };
          }

          let sources = current.snapshot.sources.map((row) => {
            if (row.sourceNumber !== sourceNumber) {
              return row;
            }
            return { ...row, ...(patch.controller ?? {}) };
          });

          if (patch.airplay) {
            sources = sources.map((row) => ({
              ...row,
              isAirplay: row.sourceNumber === sourceNumber ? 1 : 0,
            }));
          }

          return {
            ...current,
            savingSource: null,
            snapshot: {
              ...current.snapshot,
              sources,
            },
          };
        });

        showToast(
          result?.shairportRestartRequired
            ? "AirPlay source updated; zones will restart when the supervisor applies changes."
            : `Source ${sourceNumber} settings saved.`,
          "success"
        );
      } catch (error) {
        setState((current) => ({
          ...current,
          savingSource: null,
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

  const playback = ((): AudioPlaybackView | null => {
    const snapshot = state.snapshot;
    if (!snapshot) {
      return null;
    }
    const ownerZoneId = getDacOwnerZoneId(snapshot.pcm);
    if (ownerZoneId <= 0) {
      return null;
    }

    const ownerZone = snapshot.zones.find((zone) => zone.zoneNumber === ownerZoneId);
    const airplaySource = snapshot.sources.find((source) => source.isAirplay === 1);
    const { title, artist, album, clientName, clientModel } = snapshot.pcm.metadata;
    const sourceParts: string[] = [];
    if (airplaySource?.sourceNumber !== undefined) {
      sourceParts.push(`Source ${airplaySource.sourceNumber}`);
    } else if (airplaySource?.name) {
      sourceParts.push(airplaySource.name);
    }

    const config = getAppConfig();
    const coverUrl = snapshot.pcm.hasCoverArt
      ? `${config.apiBaseUrl}/api/audio/playback/cover/${ownerZoneId}?v=${snapshot.pcm.playback?.progressSyncedAt ?? Date.now()}`
      : undefined;

    return {
      visible: true,
      zoneId: ownerZoneId,
      zoneName: ownerZone?.name ?? `Zone ${ownerZoneId}`,
      track: title,
      artist,
      album,
      clientName: clientName || clientModel,
      sourceLabel: sourceParts.length > 0 ? sourceParts.join(" • ") : undefined,
      playing: snapshot.pcm.playback?.playing ?? false,
      positionMs: snapshot.pcm.playback?.positionMs ?? 0,
      durationMs: snapshot.pcm.playback?.durationMs ?? 0,
      progressSyncedAt: snapshot.pcm.playback?.progressSyncedAt ?? Date.now(),
      volume: ownerZone?.volume ?? 0,
      coverUrl,
    };
  })();

  const sendPlaybackCommand = useCallback(
    async (command: PlaybackRemoteCommand) => {
      const snapshot = state.snapshot;
      const ownerZoneId = snapshot ? getDacOwnerZoneId(snapshot.pcm) : 0;
      if (ownerZoneId <= 0) {
        return;
      }

      const config = getAppConfig();
      try {
        const response = await fetch(`${config.apiBaseUrl}/api/audio/playback/remote`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ zoneId: ownerZoneId, command }),
        });
        const body = (await response.json()) as AudioApiResponse<{ zoneId: number; command: string }>;
        if (!body.ok) {
          throw new Error(body.error?.message ?? "Playback command failed");
        }
      } catch (error) {
        showToast(error instanceof Error ? error.message : "Playback command failed", "error");
      }
    },
    [state.snapshot]
  );

  const setPlaybackVolume = useCallback(
    async (volume: number) => {
      const snapshot = state.snapshot;
      const ownerZoneId = snapshot ? getDacOwnerZoneId(snapshot.pcm) : 0;
      if (ownerZoneId <= 0) {
        return;
      }
      await setZoneVolume(ownerZoneId, volume);
    },
    [state.snapshot, setZoneVolume]
  );

  return {
    state,
    refresh,
    saveZoneSettings,
    saveSourceSettings,
    toggleZonePower,
    setZoneVolume,
    isZoneStreamedTo,
    isZoneSendingAudio,
    nowPlaying,
    playback,
    sendPlaybackCommand,
    setPlaybackVolume,
  };
}

export { useAudioModuleState };

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
