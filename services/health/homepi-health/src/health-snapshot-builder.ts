import { execFile } from "node:child_process";
import { promisify } from "node:util";

import {
  isSocketReachable,
  legacyRpc,
  sendCommand,
} from "@homepi/core-messaging";
import {
  loadServiceRegistry,
  type ModuleRegistryEntry,
  type ServiceRegistry,
  type ServiceRegistryEntry,
} from "@homepi/core-service-registry";

const execFileAsync = promisify(execFile);

/** Health status values used across snapshots. */
export type HealthStatus =
  | "healthy"
  | "degraded"
  | "failed"
  | "offline"
  | "starting"
  | "stopping"
  | "unknown";

/** Layer status for process/readiness/domain. */
export type LayerStatus =
  | "active"
  | "inactive"
  | "failed"
  | "not_ready"
  | "ready"
  | "missing_device"
  | "unknown";

/**
 * Capability health entry for module rollups.
 */
export interface CapabilityHealth {
  /** Capability id from registry. */
  id: string;
  /** Display name. */
  displayName: string;
  /** Rollup status. */
  status: HealthStatus;
  process?: LayerStatus;
  readiness?: LayerStatus;
  domain?: LayerStatus;
  userMessage?: string;
  developerMessage?: string;
  lastUpdated: string;
}

/**
 * Module health rollup for client-facing modules.
 */
export interface ModuleHealth {
  /** Module id. */
  module: string;
  /** Display name. */
  displayName: string;
  /** Icon path. */
  icon: string;
  /** Rollup status. */
  status: HealthStatus;
  /** True when the module facade is not yet installed. */
  planned?: boolean;
  userMessage?: string;
  stillWorks?: string[];
  availableActions?: string[];
  capabilities: CapabilityHealth[];
  lastUpdated: string;
}

/**
 * Platform infrastructure health entry.
 */
export interface PlatformHealthEntry {
  /** Service or component name. */
  name: string;
  /** Status. */
  status: HealthStatus;
  userMessage?: string;
  lastUpdated: string;
}

/**
 * Per-service layered health entry.
 */
export interface ServiceHealthEntry {
  /** Service name. */
  service: string;
  /** Owning module. */
  module: string;
  /** Rollup status. */
  status: HealthStatus;
  process?: LayerStatus;
  readiness?: LayerStatus;
  domain?: LayerStatus;
  userMessage?: string;
  developerMessage?: string;
  stillWorks?: string[];
  availableActions?: string[];
  lastUpdated: string;
}

/**
 * Full system health snapshot from homepi-health.
 */
export interface SystemHealthSnapshot {
  /** Snapshot timestamp. */
  checkedAt: string;
  /** Correlation id for the snapshot request. */
  correlationId?: string;
  /** Whether this service produced the snapshot successfully. */
  healthServiceReachable: boolean;
  /** Client-facing module rollups. */
  modules: ModuleHealth[];
  /** Platform infrastructure entries. */
  platform: PlatformHealthEntry[];
  /** All registered services. */
  services: ServiceHealthEntry[];
}

const CAPABILITY_LABELS: Record<string, string> = {
  "zone-control": "Zone Control",
  airplay: "AirPlay",
  "pcm-routing": "PCM Routing",
  paging: "Paging",
  "contact-detection": "Contact Detection",
  "tamper-fault": "Tamper / Fault",
  "homekit-bridge": "HomeKit Bridge",
};

/**
 * Builds a user-facing evidence line for a healthy or probed service.
 * @param entry - Service health entry.
 * @returns Evidence message for UI display.
 */
export function buildHealthEvidenceMessage(entry: ServiceHealthEntry): string {
  if (entry.userMessage) {
    return entry.userMessage;
  }

  const parts: string[] = [];
  if (entry.process === "active") {
    parts.push("Process active");
  }
  if (entry.readiness === "ready") {
    parts.push("Command socket ready");
  }
  if (entry.domain === "ready") {
    parts.push("Domain checks passing");
  } else if (entry.readiness === "ready" && entry.domain === "unknown") {
    parts.push("Responding on command socket");
  }

  if (parts.length > 0) {
    return parts.join(" · ");
  }

  if (entry.status === "healthy") {
    if (entry.process === "active") {
      return "Process active · Serving requests";
    }
    return "Operating normally";
  }

  return `${entry.service} is ${entry.status}`;
}

/**
 * Picks the primary service entry used to describe a capability's health.
 * @param related - Related service health entries.
 * @param registry - Service registry.
 * @param capabilityId - Capability id.
 * @returns Primary service health entry when available.
 */
function pickPrimaryServiceEntry(
  related: ServiceHealthEntry[],
  registry: ServiceRegistry,
  capabilityId: string
): ServiceHealthEntry | undefined {
  if (related.length === 0) {
    return undefined;
  }

  const registryEntries = registry.services.filter(
    (service) =>
      related.some((entry) => entry.service === service.name) &&
      service.capabilitiesAffected.includes(capabilityId)
  );
  const criticalEntry = registryEntries.find((service) => service.critical);
  if (criticalEntry) {
    return related.find((entry) => entry.service === criticalEntry.name) ?? related[0];
  }

  return related.find((entry) => entry.userMessage) ?? related[0];
}

/**
 * Reads systemd ActiveState for a unit.
 * @param unit - systemd unit name.
 * @returns Active state string or unknown.
 */
export async function getSystemdActiveState(unit: string): Promise<string> {
  try {
    const { stdout } = await execFileAsync("systemctl", ["show", unit, "--property=ActiveState", "--value"]);
    return stdout.trim() || "unknown";
  } catch {
    return "unknown";
  }
}

/**
 * Maps systemd ActiveState to process layer status.
 * @param activeState - systemd ActiveState value.
 * @returns Layer status.
 */
export function mapProcessLayer(activeState: string): LayerStatus {
  if (activeState === "active") {
    return "active";
  }
  if (activeState === "activating") {
    return "not_ready";
  }
  if (activeState === "failed") {
    return "failed";
  }
  if (activeState === "inactive" || activeState === "dead") {
    return "inactive";
  }
  return "unknown";
}

/**
 * Maps layered health to rollup status.
 * @param process - Process layer.
 * @param readiness - Readiness layer.
 * @param domain - Domain layer.
 * @returns Rollup health status.
 */
export function rollupStatus(
  process: LayerStatus,
  readiness: LayerStatus,
  domain: LayerStatus
): HealthStatus {
  if (process === "failed" || process === "inactive") {
    return "offline";
  }
  if (domain === "missing_device" || readiness === "not_ready") {
    return "degraded";
  }
  if (process === "active" && readiness === "ready" && (domain === "ready" || domain === "unknown")) {
    return "healthy";
  }
  if (process === "not_ready") {
    return "starting";
  }
  return "unknown";
}

/**
 * Resolves rollup status for a registry service, including socketless units.
 * Active services without a command socket rely on systemd as the only probe
 * (for example nqptp and the shairport supervisor).
 * @param entry - Registry service entry.
 * @param process - Process layer from systemd.
 * @param readiness - Readiness layer from domain probe.
 * @param domain - Domain layer from domain probe.
 * @returns Rollup health status.
 */
export function resolveServiceRollupStatus(
  entry: ServiceRegistryEntry,
  process: LayerStatus,
  readiness: LayerStatus,
  domain: LayerStatus
): HealthStatus {
  const status = rollupStatus(process, readiness, domain);
  if (!entry.planned && !entry.commandSocket && process === "active") {
    return "healthy";
  }
  return status;
}

/**
 * Resolves a service command socket path from the registry.
 * @param entry - Registry service entry.
 * @returns Socket path when reachable, otherwise null.
 */
export async function resolveServiceSocketPath(
  entry: ServiceRegistryEntry
): Promise<string | null> {
  if (!entry.commandSocket) {
    return null;
  }

  if (await isSocketReachable(entry.commandSocket)) {
    return entry.commandSocket;
  }

  return null;
}

/**
 * Probes a service command socket for domain health when available.
 * @param entry - Registry service entry.
 * @returns Domain and readiness layers plus optional messages.
 */
async function probeServiceDomain(entry: ServiceRegistryEntry): Promise<{
  readiness: LayerStatus;
  domain: LayerStatus;
  userMessage?: string;
  developerMessage?: string;
}> {
  if (entry.planned) {
    return {
      readiness: "not_ready",
      domain: "unknown",
      userMessage: `${entry.name} is planned but not installed yet.`,
    };
  }

  if (!entry.commandSocket) {
    return { readiness: "unknown", domain: "unknown" };
  }

  const socketPath = await resolveServiceSocketPath(entry);
  if (!socketPath) {
    const probePath = entry.commandSocket;
    return {
      readiness: "not_ready",
      domain: "unknown",
      userMessage: `${entry.name} is not responding on its command socket.`,
      developerMessage: `Socket unreachable: ${probePath}`,
    };
  }

  try {
    const legacy = await legacyRpc({
      socketPath,
      request: { method: "getHealth", correlationId: "health-probe" },
      timeoutMs: 5_000,
    });

    if (legacy.ok === true) {
      const status = String(legacy.status ?? legacy.data ?? "healthy");
      if (status.includes("degraded")) {
        return {
          readiness: "ready",
          domain: "missing_device",
          userMessage: String(legacy.userMessage ?? `${entry.name} is degraded.`),
        };
      }
      return { readiness: "ready", domain: "ready" };
    }
  } catch {
    /* fall through to v1 or snapshot probe */
  }

  try {
    const response = await sendCommand(
      socketPath,
      "homepi-health",
      entry.name,
      "ping",
      {},
      3_000
    );
    if (response.ok) {
      return { readiness: "ready", domain: "ready" };
    }
  } catch {
    /* socket exists but command failed — still partially ready */
  }

  return {
    readiness: "ready",
    domain: "unknown",
    userMessage: `${entry.name} socket is reachable.`,
  };
}

/**
 * Builds health for a single registered service.
 * @param entry - Registry service entry.
 * @returns Service health entry.
 */
export async function buildServiceHealth(entry: ServiceRegistryEntry): Promise<ServiceHealthEntry> {
  const now = new Date().toISOString();
  const activeState = entry.planned ? "inactive" : await getSystemdActiveState(entry.unit);
  const process = mapProcessLayer(activeState);
  const domainProbe = await probeServiceDomain(entry);
  const status = resolveServiceRollupStatus(
    entry,
    process,
    domainProbe.readiness,
    domainProbe.domain
  );

  return {
    service: entry.name,
    module: entry.module,
    status,
    process,
    readiness: domainProbe.readiness,
    domain: domainProbe.domain,
    userMessage:
      domainProbe.userMessage ??
      (status === "healthy"
        ? buildHealthEvidenceMessage({
            service: entry.name,
            module: entry.module,
            status,
            process,
            readiness: domainProbe.readiness,
            domain: domainProbe.domain,
            lastUpdated: now,
          })
        : undefined),
    ...(domainProbe.developerMessage ? { developerMessage: domainProbe.developerMessage } : {}),
    lastUpdated: now,
  };
}

/**
 * Builds capability health for a module.
 * @param moduleEntry - Module registry entry.
 * @param serviceHealth - Health entries for module services.
 * @returns Capability health list.
 */
export function buildModuleCapabilities(
  moduleEntry: ModuleRegistryEntry,
  serviceHealth: ServiceHealthEntry[],
  registry: ServiceRegistry
): CapabilityHealth[] {
  const now = new Date().toISOString();

  return moduleEntry.capabilities.map((capabilityId) => {
    const related = serviceHealth.filter((entry) => {
      const registryEntry = registry.services.find((service) => service.name === entry.service);
      return (
        registryEntry?.module === moduleEntry.id &&
        registryEntry.capabilitiesAffected.includes(capabilityId)
      );
    });

    const worst =
      related.length === 0
        ? ("unknown" as HealthStatus)
        : related.reduce<HealthStatus>((acc, entry) => {
            if (entry.status === "offline" || entry.status === "failed") {
              return "offline";
            }
            if (entry.status === "degraded") {
              return acc === "healthy" ? "degraded" : acc;
            }
            return acc;
          }, "healthy");

    const problemEntry = related.find(
      (entry) => entry.status === "degraded" || entry.status === "offline" || entry.status === "failed"
    );
    const primaryEntry = pickPrimaryServiceEntry(related, registry, capabilityId);
    const status = moduleEntry.planned ? "offline" : worst;
    const evidenceEntry = problemEntry ?? (status === "healthy" ? primaryEntry : undefined);
    const userMessage = moduleEntry.planned
      ? `${moduleEntry.facadeService} is planned but not installed yet.`
      : evidenceEntry
        ? buildHealthEvidenceMessage(evidenceEntry)
        : undefined;

    return {
      id: capabilityId,
      displayName: CAPABILITY_LABELS[capabilityId] ?? capabilityId,
      status,
      ...(userMessage ? { userMessage } : {}),
      ...(evidenceEntry?.process ? { process: evidenceEntry.process } : {}),
      ...(evidenceEntry?.readiness ? { readiness: evidenceEntry.readiness } : {}),
      ...(evidenceEntry?.domain ? { domain: evidenceEntry.domain } : {}),
      lastUpdated: evidenceEntry?.lastUpdated ?? now,
    };
  });
}

/**
 * Builds module rollup health.
 * @param moduleEntry - Module registry entry.
 * @param serviceHealth - All service health entries.
 * @returns Module health rollup.
 */
export function buildModuleHealth(
  moduleEntry: ModuleRegistryEntry,
  serviceHealth: ServiceHealthEntry[],
  registry: ServiceRegistry
): ModuleHealth {
  const now = new Date().toISOString();
  const capabilities = buildModuleCapabilities(moduleEntry, serviceHealth, registry);

  let status: HealthStatus = "healthy";
  for (const capability of capabilities) {
    if (capability.status === "offline" || capability.status === "failed") {
      status = "degraded";
      break;
    }
    if (capability.status === "degraded" && status === "healthy") {
      status = "degraded";
    }
  }

  if (moduleEntry.planned) {
    status = "offline";
  }

  const degradedCapability = capabilities.find(
    (capability) => capability.status === "degraded" || capability.status === "offline"
  );

  return {
    module: moduleEntry.id,
    displayName: moduleEntry.displayName,
    icon: moduleEntry.icon,
    status,
    ...(moduleEntry.planned ? { planned: true } : {}),
    capabilities,
    lastUpdated: now,
    ...(degradedCapability?.userMessage ? { userMessage: degradedCapability.userMessage } : {}),
    ...(status === "degraded"
      ? { stillWorks: capabilities.filter((c) => c.status === "healthy").map((c) => c.displayName) }
      : {}),
  };
}

/**
 * Builds platform infrastructure health entries.
 * @param serviceHealth - All service health entries.
 * @returns Platform entries.
 */
export function buildPlatformHealth(serviceHealth: ServiceHealthEntry[]): PlatformHealthEntry[] {
  const platformNames = [
    "homepi-backend",
    "homepi-health",
    "homepi-broker",
    "homepi-usb-devices",
  ];

  return platformNames.map((name) => {
    const entry = serviceHealth.find((service) => service.service === name);
    const status = entry?.status ?? "unknown";
    const userMessage =
      entry && status === "healthy"
        ? buildHealthEvidenceMessage(entry)
        : entry?.userMessage;

    return {
      name,
      status,
      ...(userMessage ? { userMessage } : {}),
      lastUpdated: entry?.lastUpdated ?? new Date().toISOString(),
    };
  });
}

/**
 * Builds a full system health snapshot.
 * @param correlationId - Optional correlation id.
 * @param registry - Optional registry override.
 * @returns System health snapshot.
 */
export async function buildSystemHealthSnapshot(
  correlationId?: string,
  registry: ServiceRegistry = loadServiceRegistry()
): Promise<SystemHealthSnapshot> {
  const services = await Promise.all(registry.services.map((entry) => buildServiceHealth(entry)));
  const modules = registry.modules.map((entry) => buildModuleHealth(entry, services, registry));
  const platform = buildPlatformHealth(services);

  return {
    checkedAt: new Date().toISOString(),
    correlationId,
    healthServiceReachable: true,
    modules,
    platform,
    services,
  };
}
