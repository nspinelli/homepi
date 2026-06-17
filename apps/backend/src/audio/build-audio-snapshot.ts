import type { ServiceConfig } from "@homepi/core-config";

import type { HifiSerialClient } from "../hifi-serial/hifi-serial-client.js";
import type { MetadataClient } from "../metadata/metadata-client.js";
import type { PcmRouterClient } from "../pcm-router/pcm-router-client.js";
import type { SystemStatusSnapshot } from "../types/system-status-types.js";
import type { ShairportRemoteClient } from "./shairport-remote-client.js";

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
    metadataClient: MetadataClient;
    shairportRemote: ShairportRemoteClient;
    systemStatus: SystemStatusSnapshot;
  },
  correlationId: string
): Promise<AudioSnapshot> {
  const [hifiSnapshot, shairportSettings, pcmSnapshot, metadataSnapshot, hifiHealth] =
    await Promise.all([
    deps.hifiClient.getSnapshot(correlationId).catch(() => ({})),
    deps.hifiClient
      .getShairportZoneSettings(correlationId)
      .catch(() => ({ shairportZoneSettings: [] as unknown[] })),
    deps.pcmClient.getSnapshot(correlationId).catch(() => null),
    deps.metadataClient.getSnapshot(correlationId).catch(() => null),
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

  const ownerZoneId = pcmSnapshot?.ownerZoneId ?? 0;
  let clientName = metadataSnapshot?.clientName;
  if (!clientName && ownerZoneId > 0) {
    clientName =
      (await deps.shairportRemote.fetchRetainedTopic(ownerZoneId, "client_name")) ??
      (await deps.shairportRemote.fetchRetainedTopic(ownerZoneId, "client_model")) ??
      undefined;
  }

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
      metadata: {
        title: metadataSnapshot?.title,
        artist: metadataSnapshot?.artist,
        album: metadataSnapshot?.album,
        clientName,
      },
      playback: {
        playing: metadataSnapshot?.playing ?? false,
        positionMs: metadataSnapshot?.positionMs ?? 0,
        durationMs: metadataSnapshot?.durationMs ?? 0,
      },
      hasCoverArt: metadataSnapshot?.hasCoverArt,
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
