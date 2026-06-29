import { sendCommand } from "@homepi/core-messaging";

/** Default homepi-health socket path. */
export const DEFAULT_HEALTH_SOCKET = "/run/homepi/health/health.sock";

/**
 * Module health entry from homepi-health snapshot.
 */
export interface ModuleHealth {
  module: string;
  displayName: string;
  icon: string;
  status: string;
  /** True when the module facade is not yet installed. */
  planned?: boolean;
  userMessage?: string;
  stillWorks?: string[];
  availableActions?: string[];
  capabilities: CapabilityHealth[];
  lastUpdated: string;
}

/**
 * Capability health entry from homepi-health snapshot.
 */
export interface CapabilityHealth {
  id: string;
  displayName: string;
  status: string;
  process?: string;
  readiness?: string;
  domain?: string;
  userMessage?: string;
  developerMessage?: string;
  lastUpdated: string;
}

/**
 * Platform health entry from homepi-health snapshot.
 */
export interface PlatformHealthEntry {
  name: string;
  status: string;
  userMessage?: string;
  lastUpdated: string;
}

/**
 * Service health entry from homepi-health snapshot.
 */
export interface ServiceHealthEntry {
  service: string;
  module: string;
  status: string;
  process?: string;
  readiness?: string;
  domain?: string;
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
  checkedAt: string;
  correlationId?: string;
  healthServiceReachable: boolean;
  modules: ModuleHealth[];
  platform: PlatformHealthEntry[];
  services: ServiceHealthEntry[];
}

/**
 * Client for the homepi-health observer socket.
 */
export class HealthClient {
  /**
   * @param socketPath - Unix socket path for homepi-health.
   */
  constructor(private readonly socketPath: string = DEFAULT_HEALTH_SOCKET) {}

  /**
   * Fetches the current system health snapshot.
   * @param correlationId - Request correlation id.
   * @returns Health snapshot or degraded fallback when unreachable.
   */
  async getSnapshot(correlationId: string): Promise<SystemHealthSnapshot> {
    try {
      const response = await sendCommand(
        this.socketPath,
        "homepi-backend",
        "homepi-health",
        "health.snapshot",
        {},
        10_000
      );

      if (!response.ok || !response.result?.snapshot) {
        return this.unreachableSnapshot(
          correlationId,
          "Health monitoring returned an error response."
        );
      }

      return response.result.snapshot as SystemHealthSnapshot;
    } catch (error) {
      return this.unreachableSnapshot(
        correlationId,
        error instanceof Error ? error.message : "Health monitoring is unavailable."
      );
    }
  }

  private unreachableSnapshot(
    correlationId: string,
    _developerMessage: string
  ): SystemHealthSnapshot {
    return {
      checkedAt: new Date().toISOString(),
      correlationId,
      healthServiceReachable: false,
      modules: [],
      platform: [
        {
          name: "homepi-health",
          status: "offline",
          userMessage:
            "Health monitoring is unavailable. Module status may be stale until the health service recovers.",
          lastUpdated: new Date().toISOString(),
        },
      ],
      services: [],
    };
  }
}
