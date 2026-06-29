/**
 * Client-facing module facade entry from the service registry.
 */
export interface ModuleRegistryEntry {
  /** Module key, e.g. `audio` or `contact-sensors`. */
  id: string;
  /** User-facing label. */
  displayName: string;
  /** systemd service name for the facade. */
  facadeService: string;
  /** Public command socket path. */
  commandSocket: string;
  /** Frontend public asset path for the module icon. */
  icon: string;
  /** Capability IDs owned by this module. */
  capabilities: string[];
  /** True when the facade is not yet installed. */
  planned?: boolean;
}

/**
 * Individual service entry from the service registry.
 */
export interface ServiceRegistryEntry {
  /** Service name, e.g. `homepi-pcm-router`. */
  name: string;
  /** Owning module or `platform`. */
  module: string;
  /** systemd unit file name. */
  unit: string;
  /** Runtime role classification. */
  role:
    | "module-facade"
    | "hardware-controller"
    | "data-plane"
    | "platform"
    | "event-fanout"
    | "health-observer"
    | "api-gateway";
  /** Command socket path when applicable. */
  commandSocket?: string;
  /** Whether failure degrades the whole module critically. */
  critical: boolean;
  /** Capability IDs affected by this service. */
  capabilitiesAffected: string[];
  /** User-facing failure grouping key. */
  userFacingFailureCategory: string;
  /** True when not yet installed. */
  planned?: boolean;
  /** Pre-migration flat socket path. */
  legacySocket?: string;
}

/**
 * Full HomePi service registry document.
 */
export interface ServiceRegistry {
  /** Schema version. */
  version: number;
  /** Client-facing module facades. */
  modules: ModuleRegistryEntry[];
  /** All registered services. */
  services: ServiceRegistryEntry[];
}
