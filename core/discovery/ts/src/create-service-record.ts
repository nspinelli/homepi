import type { ServiceDiscovery } from "./discovery-types.js";

/**
 * Creates a discoverable service record.
 * @param params - Service discovery fields.
 * @returns Service discovery record.
 */
export function createServiceRecord(params: {
  service: string;
  host: string;
  capabilities: string[];
  port?: number;
  socketPath?: string;
}): ServiceDiscovery {
  return {
    service: params.service,
    host: params.host,
    capabilities: params.capabilities,
    port: params.port,
    socketPath: params.socketPath,
  };
}
