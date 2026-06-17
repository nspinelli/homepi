import type { EventEnvelope } from "@homepi/core-events";

import type { SystemStatusSnapshot } from "../types/system-status-types.js";
import {
  mapHifiSerialFromPayload,
  mapPcmRouterFromDacState,
  mapPcmRouterFromPayload,
  mapUsbDevicesFromPayload,
} from "./service-status-mappers.js";

const SERVICE_STATUS_TOPIC = "system.service";
const PCM_TOPIC = "modules.pcm";
const PCM_SNAPSHOT_TOPIC = "modules.pcm.snapshot";

/**
 * Maps a native service event envelope to a partial system status patch.
 * @param envelope - Validated event envelope.
 * @returns Partial status patch or null when the envelope does not affect dashboard health.
 */
export function mapEnvelopeToStatusPatch(
  envelope: EventEnvelope
): Partial<SystemStatusSnapshot> | null {
  const payload = (envelope.payload ?? {}) as Record<string, unknown>;

  if (envelope.topic === SERVICE_STATUS_TOPIC) {
    if (envelope.source === "homepi-hifi-serial") {
      return { hifiSerial: mapHifiSerialFromPayload(payload) };
    }
    if (envelope.source === "homepi-usb-devices") {
      return { usbDevices: mapUsbDevicesFromPayload(payload) };
    }
    if (envelope.source === "homepi-pcm-router") {
      const pcm = mapPcmRouterFromPayload(payload, envelope.event);
      return pcm ? { pcmRouter: pcm } : null;
    }
  }

  if (
    envelope.source === "homepi-pcm-router" &&
    (envelope.topic === PCM_TOPIC || envelope.topic === PCM_SNAPSHOT_TOPIC)
  ) {
    if (envelope.event === "pcm_router_snapshot" && typeof payload.dacState === "string") {
      return { pcmRouter: mapPcmRouterFromDacState(payload.dacState) };
    }
    const pcm = mapPcmRouterFromPayload(payload, envelope.event);
    return pcm ? { pcmRouter: pcm } : null;
  }

  return null;
}
