import type { EventEnvelope } from "@homepi/core-events";

import type { MetadataClient } from "../metadata/metadata-client.js";
import type { PcmRouterClient } from "../pcm-router/pcm-router-client.js";

/**
 * Cached broker payloads used to hydrate REST snapshots without per-service sockets.
 */
export class AudioBrokerSnapshotStore {
  private hifiSnapshot: Record<string, unknown> | null = null;
  private pcmSnapshot: Awaited<ReturnType<PcmRouterClient["getSnapshot"]>> | null = null;
  private metadataSnapshot: Awaited<ReturnType<MetadataClient["getSnapshot"]>> | null = null;

  /**
   * Records a broker envelope when it carries snapshot data.
   * @param envelope - Broker event envelope.
   */
  ingest(envelope: EventEnvelope): void {
    const payload = envelope.payload;
    if (!payload || typeof payload !== "object") {
      return;
    }

    if (
      envelope.topic === "modules.audio.snapshot" &&
      envelope.event === "audio_state_snapshot"
    ) {
      this.hifiSnapshot = payload as Record<string, unknown>;
      return;
    }

    if (
      (envelope.topic === "modules.pcm.snapshot" || envelope.topic === "modules.pcm") &&
      envelope.event === "pcm_router_snapshot"
    ) {
      this.pcmSnapshot = payload as unknown as Awaited<
        ReturnType<PcmRouterClient["getSnapshot"]>
      >;
      return;
    }

    if (
      envelope.topic === "modules.pcm.routing" &&
      (envelope.event === "owner_changed" || envelope.event === "owner_pending")
    ) {
      const partial = payload as Record<string, unknown>;
      const current = this.pcmSnapshot;
      const base = current ?? {
        ownerZoneId: 0,
        activeStack: [] as number[],
        dacState: "unknown",
      };
      this.pcmSnapshot = {
        ...base,
        ownerZoneId:
          typeof partial.ownerZoneId === "number" ? partial.ownerZoneId : base.ownerZoneId,
        activeStack: Array.isArray(partial.activeStack)
          ? (partial.activeStack as number[])
          : base.activeStack,
        pendingOwnerZoneId:
          typeof partial.pendingOwnerZoneId === "number"
            ? partial.pendingOwnerZoneId
            : envelope.event === "owner_pending" && typeof partial.ownerZoneId === "number"
              ? partial.ownerZoneId
              : base.pendingOwnerZoneId,
      };
      return;
    }

    if (
      (envelope.topic === "modules.pcm.snapshot" || envelope.topic === "modules.pcm") &&
      envelope.event === "routing_changed"
    ) {
      const partial = payload as Record<string, unknown>;
      const current = this.pcmSnapshot;
      const base = current ?? {
        ownerZoneId: 0,
        activeStack: [] as number[],
        dacState: "unknown",
      };
      this.pcmSnapshot = {
        ...base,
        ownerZoneId:
          typeof partial.ownerZoneId === "number" ? partial.ownerZoneId : base.ownerZoneId,
        activeStack: Array.isArray(partial.activeStack)
          ? (partial.activeStack as number[])
          : base.activeStack,
        pendingOwnerZoneId:
          typeof partial.pendingOwnerZoneId === "number"
            ? partial.pendingOwnerZoneId
            : base.pendingOwnerZoneId,
        dacState: typeof partial.dacState === "string" ? partial.dacState : base.dacState,
      };
      return;
    }

    if (
      envelope.topic === "modules.metadata.snapshot" &&
      (envelope.event === "metadata_snapshot" ||
        envelope.event === "metadata_track_changed" ||
        envelope.event === "metadata_owner_changed")
    ) {
      this.metadataSnapshot = payload as unknown as Awaited<
        ReturnType<MetadataClient["getSnapshot"]>
      >;
      return;
    }

    if (
      envelope.topic === "modules.metadata.now_playing" &&
      (envelope.event === "metadata_track_changed" ||
        envelope.event === "metadata_owner_changed")
    ) {
      this.metadataSnapshot = payload as unknown as Awaited<
        ReturnType<MetadataClient["getSnapshot"]>
      >;
    }
  }

  /**
   * Returns the cached Hi-Fi snapshot when present.
   * @returns Cached snapshot object.
   */
  getHifiSnapshot(): Record<string, unknown> | null {
    return this.hifiSnapshot;
  }

  /**
   * Returns the cached PCM router snapshot when present.
   * @returns Cached PCM snapshot.
   */
  getPcmSnapshot(): Awaited<ReturnType<PcmRouterClient["getSnapshot"]>> | null {
    return this.pcmSnapshot;
  }

  /**
   * Returns the cached metadata snapshot when present.
   * @returns Cached metadata snapshot.
   */
  getMetadataSnapshot(): Awaited<ReturnType<MetadataClient["getSnapshot"]>> | null {
    return this.metadataSnapshot;
  }

  /**
   * Returns true when any audio snapshot cache entry is populated.
   * @returns True when broker cache can hydrate REST.
   */
  hasAnySnapshot(): boolean {
    return this.hifiSnapshot !== null || this.pcmSnapshot !== null || this.metadataSnapshot !== null;
  }
}
