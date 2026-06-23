import type { EventEnvelope } from "@homepi/core-events";

/** Broker topic patterns used for audio UI SSE. */
export const BROKER_AUDIO_TOPICS = [
  "core.service",
  "modules.hifi.zone",
  "modules.hifi.controller",
  "modules.hifi.command_status",
  "modules.pcm.routing",
  "modules.pcm.snapshot",
  "modules.pcm",
  "modules.metadata.snapshot",
  "modules.metadata.now_playing",
  "modules.metadata.cover_art",
  "modules.metadata.playback",
  "modules.metadata.history",
  "modules.audio.state",
  "modules.audio.snapshot",
  "modules.zone.config",
] as const;

/**
 * Returns whether broker-only audio SSE mode is enabled.
 * @returns True unless HOMEPI_BROKER_ONLY_AUDIO_SSE=false.
 */
export function isBrokerOnlyAudioSseEnabled(): boolean {
  return process.env.HOMEPI_BROKER_ONLY_AUDIO_SSE !== "false";
}

/**
 * Returns whether REST snapshots should prefer broker cache.
 * @returns True unless HOMEPI_BROKER_AUDIO_SNAPSHOT=false.
 */
export function isBrokerAudioSnapshotEnabled(): boolean {
  return process.env.HOMEPI_BROKER_AUDIO_SNAPSHOT !== "false";
}

/**
 * Returns true when a broker envelope should not be forwarded to SSE clients.
 * @param envelope - Broker event envelope.
 * @returns True when the envelope should be dropped.
 */
export function shouldDropBrokerEnvelope(envelope: EventEnvelope): boolean {
  if (
    envelope.topic === "modules.metadata.progress" ||
    envelope.event === "metadata_progress_updated"
  ) {
    return true;
  }
  return false;
}

/**
 * Maps broker event names to legacy SSE names the frontend already handles.
 * @param envelope - Raw broker envelope.
 * @returns Adapted envelope for UI consumers.
 */
export function adaptBrokerEnvelopeForUi(envelope: EventEnvelope): EventEnvelope {
  const adapted: EventEnvelope = { ...envelope };

  if (envelope.topic === "modules.pcm.routing" && envelope.event === "owner_changed") {
    adapted.event = "routing_changed";
    adapted.source = "homepi-pcm-router";
  }

  if (envelope.topic === "modules.pcm.snapshot" || envelope.topic === "modules.pcm") {
    adapted.source = "homepi-pcm-router";
    if (envelope.event === "pcm_router_snapshot") {
      adapted.event = "pcm_router_snapshot";
    }
  }

  if (
    envelope.topic === "modules.metadata.snapshot" ||
    envelope.topic === "modules.metadata.now_playing"
  ) {
    adapted.source = "homepi-metadata";
    if (envelope.event === "metadata_track_changed") {
      adapted.event = "metadata_snapshot";
    }
  }

  if (envelope.topic === "modules.metadata.cover_art") {
    adapted.source = "homepi-metadata";
    if (envelope.event === "cover_art_updated") {
      adapted.event = "metadata_cover_updated";
    }
  }

  if (envelope.topic === "modules.metadata.playback") {
    adapted.source = "homepi-metadata";
  }

  if (envelope.topic.startsWith("modules.hifi.")) {
    adapted.source = "homepi-hifi-serial";
  }

  return adapted;
}
