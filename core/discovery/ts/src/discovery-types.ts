/**
 * Capability shape per capability.schema.json.
 */
export interface Capability {
  /** Capability identifier. */
  id: string;
  /** Owning service or module. */
  owner: string;
  /** Human-readable description. */
  description: string;
}

/**
 * Discoverable service record per service-discovery.schema.json.
 */
export interface ServiceDiscovery {
  /** Service name. */
  service: string;
  /** Hostname or address. */
  host: string;
  /** TCP port when applicable. */
  port?: number;
  /** Unix socket path when applicable. */
  socketPath?: string;
  /** Advertised capability identifiers. */
  capabilities: string[];
}
