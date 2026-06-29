import { access } from "node:fs/promises";
import { constants } from "node:fs";

import { legacyRpc as rpc } from "@homepi/core-messaging";
import { findService, loadServiceRegistry } from "@homepi/core-service-registry";

const registry = loadServiceRegistry();

/**
 * Resolves a service command socket path from the registry.
 * @param serviceName - Registry service name.
 * @returns Socket path.
 */
async function resolveSocket(serviceName: string): Promise<string> {
  const entry = findService(registry, serviceName);
  if (!entry?.commandSocket) {
    throw new Error(`Unknown audio internal service: ${serviceName}`);
  }

  try {
    await access(entry.commandSocket, constants.F_OK);
    return entry.commandSocket;
  } catch {
    throw new Error(`No reachable socket for ${serviceName} at ${entry.commandSocket}`);
  }
}

/**
 * Proxies a legacy RPC to an internal audio service.
 * @param serviceName - Internal service name.
 * @param method - Legacy RPC method.
 * @param body - Additional request fields.
 * @returns Legacy RPC response object.
 */
export async function proxyLegacy(
  serviceName: string,
  method: string,
  body: Record<string, unknown> = {}
): Promise<Record<string, unknown>> {
  const socketPath = await resolveSocket(serviceName);
  return rpc({
    socketPath,
    request: { method, correlationId: String(body.correlationId ?? "audio-facade"), ...body },
  });
}

/**
 * Returns capability-level health for the audio module.
 * @returns Capability health map.
 */
export async function getAudioCapabilityHealth(): Promise<Record<string, unknown>> {
  const capabilities = [
    { id: "zone-control", service: "homepi-hifi-serial" },
    { id: "pcm-routing", service: "homepi-pcm-router" },
    { id: "paging", service: "homepi-audio-paging" },
    { id: "airplay", service: "homepi-shairport-supervisor" },
  ];

  const results = await Promise.allSettled(
    capabilities.map(async (capability) => {
      try {
        const health = await proxyLegacy(capability.service, "getHealth");
        return {
          id: capability.id,
          status: health.ok ? "healthy" : "degraded",
          userMessage: health.userMessage,
        };
      } catch (error) {
        return {
          id: capability.id,
          status: "offline",
          userMessage:
            error instanceof Error
              ? error.message
              : `${capability.service} is unavailable.`,
        };
      }
    })
  );

  return {
    module: "audio",
    capabilities: results.map((result) =>
      result.status === "fulfilled" ? result.value : { id: "unknown", status: "offline" }
    ),
  };
}
