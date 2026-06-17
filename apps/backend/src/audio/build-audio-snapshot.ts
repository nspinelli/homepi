import type { ServiceConfig } from "@homepi/core-config";

import type { HifiSerialClient } from "../hifi-serial/hifi-serial-client.js";
import type { PcmRouterClient } from "../pcm-router/pcm-router-client.js";
import type { SystemStatusSnapshot } from "../types/system-status-types.js";

/**
 * Aggregated audio module snapshot for initial page load.
 */
export interface AudioSnapshot {
  controller: Record<string, unknown>;
  zones: unknown[];
  sources: unknown[];
  groups: unknown[];
  shairportZoneSettings: unknown[];
  pcm: {
    ownerZoneId: number;
    activeStack: number[];
    dacState: string;
    profileMode?: string;
    profileStatus?: string;
    loopbackProfile?: {
      sampleRate: number;
      channels: number;
      sampleFormat: string;
    };
    dacProfile?: {
      sampleRate: number;
      channels: number;
      sampleFormat: string;
    };
    profileRevision?: number;
    profileSource?: string;
    audioBridgeState?: string;
    metadata: {
      title?: string;
      artist?: string;
      album?: string;
      clientName?: string;
    };
    playback: {
      playing: boolean;
      positionMs: number;
      durationMs: number;
    };
    hasCoverArt?: boolean;
  };
  services: {
    hifiSerial: string;
    shairport: string;
    pcmRouter: string;
    nqptp: string;
    metadata: string;
  };
  hifiConnected: boolean;
}

/**
 * Builds the audio dashboard snapshot from REST and socket sources.
 * @param deps - Snapshot dependencies.
 * @param correlationId - Request correlation id.
 * @returns Combined audio snapshot.
 */
export async function buildAudioSnapshot(
  deps: {
    config: ServiceConfig;
    hifiClient: HifiSerialClient;
    pcmClient: PcmRouterClient;
    systemStatus: SystemStatusSnapshot;
  },
  correlationId: string
): Promise<AudioSnapshot> {
  const [hifiSnapshot, shairportSettings, pcmSnapshot, hifiHealth] = await Promise.all([
    deps.hifiClient.getSnapshot(correlationId).catch(() => ({})),
    deps.hifiClient
      .getShairportZoneSettings(correlationId)
      .catch(() => ({ shairportZoneSettings: [] as unknown[] })),
    deps.pcmClient.getSnapshot(correlationId).catch(() => null),
    deps.hifiClient.getHealth(correlationId).catch(() => null),
  ]);

  const controller =
    typeof hifiSnapshot === "object" &&
    hifiSnapshot !== null &&
    "controller" in hifiSnapshot
      ? (hifiSnapshot.controller as Record<string, unknown>)
      : {};

  const zones = Array.isArray((hifiSnapshot as { zones?: unknown }).zones)
    ? ((hifiSnapshot as { zones: unknown[] }).zones ?? [])
    : [];

  const sources = Array.isArray((hifiSnapshot as { sources?: unknown }).sources)
    ? ((hifiSnapshot as { sources: unknown[] }).sources ?? [])
    : [];

  const groups = Array.isArray((hifiSnapshot as { groups?: unknown }).groups)
    ? ((hifiSnapshot as { groups: unknown[] }).groups ?? [])
    : [];

  return {
    controller,
    zones,
    sources,
    groups,
    shairportZoneSettings: shairportSettings.shairportZoneSettings,
    pcm: {
      ownerZoneId: pcmSnapshot?.ownerZoneId ?? 0,
      activeStack: pcmSnapshot?.activeStack ?? [],
      dacState: pcmSnapshot?.dacState ?? "unknown",
      profileMode: pcmSnapshot?.profileMode,
      profileStatus: pcmSnapshot?.profileStatus,
      loopbackProfile: pcmSnapshot?.loopbackProfile,
      dacProfile: pcmSnapshot?.dacProfile,
      profileRevision: pcmSnapshot?.profileRevision,
      profileSource: pcmSnapshot?.profileSource,
      audioBridgeState: pcmSnapshot?.audioBridgeState,
      metadata: {},
      playback: {
        playing: false,
        positionMs: 0,
        durationMs: 0,
      },
    },
    services: {
      hifiSerial: deps.systemStatus.hifiSerial,
      shairport: deps.systemStatus.shairport,
      pcmRouter: deps.systemStatus.pcmRouter,
      nqptp: deps.systemStatus.nqptp,
      metadata: deps.systemStatus.metadata,
    },
    hifiConnected: hifiHealth?.connected === true,
  };
}
