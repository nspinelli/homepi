import { createEventEnvelope } from "@homepi/core-events";
import type { EventEnvelope } from "@homepi/core-events";

import type { AudioRealtimeFrame } from "./audio/audio-realtime-bridge.js";
import type { MetadataSnapshotPayload } from "./metadata/metadata-client.js";

/**
 * Builds a metadata snapshot envelope for a freshly connected SSE client.
 * @param snapshot - Live metadata from the metadata service.
 * @param correlationId - Request correlation identifier.
 * @returns Metadata snapshot envelope.
 */
export function buildMetadataSnapshotEnvelope(
  snapshot: MetadataSnapshotPayload,
  correlationId: string
): EventEnvelope {
  return createEventEnvelope({
    source: "homepi-metadata",
    topic: "modules.metadata.now_playing",
    event: "metadata_snapshot",
    correlationId,
    timestamp: snapshot.updatedAt ?? new Date().toISOString(),
    payload: {
      ownerZoneId: snapshot.ownerZoneId,
      zoneId: snapshot.zoneId,
      title: snapshot.title ?? "",
      artist: snapshot.artist ?? "",
      album: snapshot.album ?? "",
      clientName: snapshot.clientName ?? "",
      clientModel: snapshot.clientModel ?? "",
      coverArtUrl: snapshot.coverArtUrl ?? "",
      coverArtId: snapshot.coverArtId ?? "",
      metadataQuality: snapshot.metadataQuality ?? "",
      trackId: snapshot.trackId ?? "",
      playing: snapshot.playing,
      positionMs: snapshot.positionMs,
      durationMs: snapshot.durationMs,
      hasCoverArt: snapshot.hasCoverArt,
      updatedAt: snapshot.updatedAt ?? new Date().toISOString(),
    },
  });
}

/**
 * Builds an audio realtime progress envelope for a freshly connected SSE client.
 * @param frame - Latest progress frame from audio-realtime.sock.
 * @param correlationId - Request correlation identifier.
 * @returns Audio realtime envelope.
 */
export function buildAudioRealtimeEnvelope(
  frame: AudioRealtimeFrame,
  correlationId: string
): EventEnvelope {
  const receivedAtMs = frame.wallTime ? Date.parse(frame.wallTime) : Date.now();
  return createEventEnvelope({
    source: "homepi-backend",
    topic: "modules.audio.realtime",
    event: "audio.realtime",
    correlationId,
    timestamp: frame.wallTime ?? new Date().toISOString(),
    payload: {
      ownerZoneId: frame.ownerZoneId,
      zoneId: frame.ownerZoneId,
      trackId: frame.trackId,
      playing: frame.playing,
      positionMs: frame.positionMs,
      durationMs: frame.durationMs,
      receivedAtMs,
      progressSyncedAt: receivedAtMs,
      progressSource: frame.progressSource,
    },
  });
}
