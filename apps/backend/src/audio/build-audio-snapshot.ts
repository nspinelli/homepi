import type { ServiceConfig } from "@homepi/core-config";

import type { HifiSerialClient } from "../hifi-serial/hifi-serial-client.js";
import type { MetadataClient } from "../metadata/metadata-client.js";
import type { PcmRouterClient } from "../pcm-router/pcm-router-client.js";
import type { HealthClient } from "../health/health-client.js";
import { deriveAudioServiceStatus } from "./derive-audio-service-status.js";
import type { ShairportRemoteClient } from "./shairport-remote-client.js";
import { readAudioRealtimeSnapshot } from "./read-audio-realtime-snapshot.js";
import { isBrokerAudioSnapshotEnabled } from "./audio-ui-bridge.js";
import type { AudioBrokerSnapshotStore } from "./audio-broker-snapshot-store.js";
import { resolveRuntimeSocketPaths } from "../runtime-socket-paths.js";

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
    pendingOwnerZoneId?: number;
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
      clientModel?: string;
      trackId?: string;
      coverArtId?: string;
      coverArtUrl?: string;
      updatedAt?: string;
    };
    playback: {
      playing: boolean;
      positionMs: number;
      durationMs: number;
      progressSyncedAt?: number;
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
    healthClient: HealthClient;
    brokerSnapshotStore?: AudioBrokerSnapshotStore;
  },
  correlationId: string
): Promise<AudioSnapshot> {
  const useBrokerCache =
    isBrokerAudioSnapshotEnabled() && deps.brokerSnapshotStore?.hasAnySnapshot() === true;

  const [hifiSnapshot, shairportSettings, pcmSnapshot, metadataSnapshot, hifiHealth, healthSnapshot] =
    await Promise.all([
    useBrokerCache && deps.brokerSnapshotStore?.getHifiSnapshot()
      ? Promise.resolve(deps.brokerSnapshotStore.getHifiSnapshot())
      : deps.hifiClient.getSnapshot(correlationId).catch(() => ({})),
    deps.hifiClient
      .getShairportZoneSettings(correlationId)
      .catch(() => ({ shairportZoneSettings: [] as unknown[] })),
    useBrokerCache && deps.brokerSnapshotStore?.getPcmSnapshot()
      ? Promise.resolve(deps.brokerSnapshotStore.getPcmSnapshot())
      :     deps.pcmClient.getSnapshot(correlationId).catch(() => null),
    deps.metadataClient.getSnapshot(correlationId).catch(() => null),
    deps.hifiClient.getHealth(correlationId).catch(() => null),
    deps.healthClient.getSnapshot(correlationId).catch(() => null),
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
  const activeStack = pcmSnapshot?.activeStack ?? [];
  const metadataOwnerZoneId =
    metadataSnapshot?.ownerZoneId ?? metadataSnapshot?.zoneId ?? 0;
  const resolvedOwnerZoneId = ownerZoneId > 0 ? ownerZoneId : metadataOwnerZoneId;
  const resolvedActiveStack =
    activeStack.length > 0
      ? activeStack
      : metadataOwnerZoneId > 0
        ? [metadataOwnerZoneId]
        : [];
  const hasActiveRoute = resolvedOwnerZoneId > 0 || resolvedActiveStack.length > 0;
  const hasNowPlayingMetadata =
    Boolean(
      metadataSnapshot?.title?.trim() ||
        metadataSnapshot?.artist?.trim() ||
        metadataSnapshot?.clientName?.trim() ||
        metadataSnapshot?.playing
    ) || metadataOwnerZoneId > 0;
  const showNowPlaying = hasActiveRoute || hasNowPlayingMetadata;
  let durationMs = metadataSnapshot?.durationMs ?? 0;

  let positionMs = metadataSnapshot?.positionMs ?? 0;
  let playing = metadataSnapshot?.playing ?? false;
  let progressSyncedAt: number | undefined;
  const realtimeSocketPath = resolveRuntimeSocketPaths(
    deps.config.runtime.paths.socketDir
  ).audioRealtime;
  const realtimeFrame = await readAudioRealtimeSnapshot(realtimeSocketPath).catch(() => null);
  if (realtimeFrame && realtimeFrame.ownerZoneId === resolvedOwnerZoneId) {
    positionMs = realtimeFrame.positionMs;
    playing = realtimeFrame.playing || playing;
    if (realtimeFrame.durationMs > 0) {
      durationMs = realtimeFrame.durationMs;
    }
    progressSyncedAt = realtimeFrame.wallTime
      ? Date.parse(realtimeFrame.wallTime)
      : Date.now();
  }

  const coverArtUrl =
    metadataSnapshot?.hasCoverArt && metadataSnapshot?.coverArtId
      ? `/api/audio/now-playing/cover?v=sha256-${metadataSnapshot.coverArtId}`
      : metadataSnapshot?.hasCoverArt
        ? "/api/audio/now-playing/cover"
        : undefined;

  return {
    controller,
    zones,
    sources,
    groups,
    shairportZoneSettings: shairportSettings.shairportZoneSettings,
    pcm: {
      ownerZoneId: resolvedOwnerZoneId,
      activeStack: resolvedActiveStack,
      pendingOwnerZoneId: pcmSnapshot?.pendingOwnerZoneId ?? 0,
      dacState: pcmSnapshot?.dacState ?? "unknown",
      profileMode: pcmSnapshot?.profileMode,
      profileStatus: pcmSnapshot?.profileStatus,
      loopbackProfile: pcmSnapshot?.loopbackProfile,
      dacProfile: pcmSnapshot?.dacProfile,
      profileRevision: pcmSnapshot?.profileRevision,
      profileSource: pcmSnapshot?.profileSource,
      audioBridgeState: pcmSnapshot?.audioBridgeState,
      metadata: showNowPlaying
        ? {
            title: metadataSnapshot?.title,
            artist: metadataSnapshot?.artist,
            album: metadataSnapshot?.album,
            clientName: metadataSnapshot?.clientName,
            clientModel: metadataSnapshot?.clientModel,
            trackId: metadataSnapshot?.trackId,
            coverArtId: metadataSnapshot?.coverArtId,
            coverArtUrl,
            updatedAt: metadataSnapshot?.updatedAt,
          }
        : {},
      playback: showNowPlaying
        ? {
            playing,
            positionMs,
            durationMs,
            progressSyncedAt: progressSyncedAt ?? Date.now(),
          }
        : {
            playing: false,
            positionMs: 0,
            durationMs: 0,
            progressSyncedAt: Date.now(),
          },
      hasCoverArt: showNowPlaying && metadataSnapshot?.hasCoverArt === true,
    },
    services: deriveAudioServiceStatus(healthSnapshot),
    hifiConnected: hifiHealth?.connected === true,
  };
}
