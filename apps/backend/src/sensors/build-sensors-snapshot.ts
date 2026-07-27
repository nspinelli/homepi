import type { SensorsClient } from "./sensors-client.js";
import type { HealthClient } from "../health/health-client.js";

/**
 * Contact sensors REST snapshot aggregate.
 */
export interface SensorsSnapshot {
  module: string;
  sensorCount: number;
  sensors: Array<Record<string, unknown>>;
  hardwareReady: boolean;
  homekitBridgeReachable: boolean;
  services: {
    sensors: string;
    homekitBridge: string;
  };
  moduleHealth?: Record<string, unknown>;
}

/**
 * Builds the contact sensors dashboard snapshot.
 * @param deps - Snapshot dependencies.
 * @param correlationId - Request correlation id.
 * @returns Combined sensors snapshot.
 */
export async function buildSensorsSnapshot(
  deps: {
    sensorsClient: SensorsClient;
    healthClient: HealthClient;
  },
  correlationId: string
): Promise<SensorsSnapshot> {
  const [facadeSnapshot, health] = await Promise.all([
    deps.sensorsClient.getSnapshot(correlationId).catch(() => ({
      module: "contact-sensors",
      sensorCount: 0,
      sensors: [],
      hardwareReady: false,
      homekitBridgeReachable: false,
    })),
    deps.healthClient.getSnapshot(correlationId),
  ]);

  const moduleHealth = health.modules.find((m) => m.module === "contact-sensors");
  const sensorsService = health.services.find((s) => s.service === "homepi-sensors");
  const homekitService = health.services.find((s) => s.service === "homepi-homekit");

  return {
    module: String(facadeSnapshot.module ?? "contact-sensors"),
    sensorCount: Number(facadeSnapshot.sensorCount ?? 0),
    sensors: Array.isArray(facadeSnapshot.sensors)
      ? (facadeSnapshot.sensors as Array<Record<string, unknown>>)
      : [],
    hardwareReady: Boolean(facadeSnapshot.hardwareReady),
    homekitBridgeReachable: Boolean(facadeSnapshot.homekitBridgeReachable),
    services: {
      sensors: sensorsService?.status ?? (moduleHealth?.status === "planned" ? "offline" : "unknown"),
      homekitBridge: homekitService?.status ?? "offline",
    },
    moduleHealth: moduleHealth
      ? {
          status: moduleHealth.status,
          userMessage: moduleHealth.userMessage,
          capabilities: moduleHealth.capabilities,
        }
      : undefined,
  };
}
