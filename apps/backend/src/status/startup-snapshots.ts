import { createCorrelationId, type Logger } from "@homepi/core-logging";

import type { HifiSerialClient } from "../hifi-serial/hifi-serial-client.js";
import type { PcmRouterClient } from "../pcm-router/pcm-router-client.js";
import { getSystemdUnitActiveState } from "../runtime/check-systemd-unit.js";
import type { UsbDevicesClient } from "../usb-devices/usb-devices-client.js";
import {
  mapHifiSerialStatus,
  mapPcmRouterFromDacState,
  mapSystemdServiceStatus,
  mapUsbDevicesStatus,
} from "./service-status-mappers.js";
import type { StatusUpdateCoordinator } from "./status-update-coordinator.js";

/**
 * One-time startup snapshot loader for a native or systemd-backed service.
 */
export interface StartupSnapshotLoader {
  /** Loader name for structured logs. */
  name: string;
  /** Loads health into the status store. */
  load: () => Promise<void>;
}

/**
 * Dependencies for building startup snapshot loaders.
 */
export interface StartupSnapshotDeps {
  /** Coordinator for initial broadcast. */
  coordinator: StatusUpdateCoordinator;
  /** USB devices socket client. */
  usbDevicesClient: UsbDevicesClient;
  /** HiFi serial socket client. */
  hifiSerialClient: HifiSerialClient;
  /** PCM router socket client. */
  pcmRouterClient: PcmRouterClient;
}

/**
 * Builds startup snapshot loaders for all dashboard services.
 * @param deps - Client and store dependencies.
 * @returns Snapshot loader definitions.
 */
export function createStartupSnapshotLoaders(deps: StartupSnapshotDeps): StartupSnapshotLoader[] {
  const { coordinator, usbDevicesClient, hifiSerialClient, pcmRouterClient } = deps;

  return [
    {
      name: "usb-devices",
      load: async () => {
        const correlationId = createCorrelationId("startup-snapshot");
        try {
          const health = await usbDevicesClient.getHealth(correlationId);
          coordinator.patchAndBroadcast(
            {
              usbDevices: mapUsbDevicesStatus(true, health.assignmentsDegraded),
            },
            "startup-snapshot"
          );
        } catch {
          coordinator.patchAndBroadcast({ usbDevices: "offline" }, "startup-snapshot");
        }
      },
    },
    {
      name: "hifi-serial",
      load: async () => {
        const correlationId = createCorrelationId("startup-snapshot");
        try {
          const health = await hifiSerialClient.getHealth(correlationId);
          coordinator.patchAndBroadcast(
            { hifiSerial: mapHifiSerialStatus(health) },
            "startup-snapshot"
          );
        } catch {
          coordinator.patchAndBroadcast({ hifiSerial: "offline" }, "startup-snapshot");
        }
      },
    },
    {
      name: "pcm-router",
      load: async () => {
        const correlationId = createCorrelationId("startup-snapshot");
        try {
          const snapshot = await pcmRouterClient.getSnapshot(correlationId);
          if (snapshot) {
            const pcm = mapPcmRouterFromDacState(snapshot.dacState);
            coordinator.patchAndBroadcast({ pcmRouter: pcm }, "startup-snapshot");
            return;
          }
        } catch {
          /* fall through to systemd probe */
        }
        try {
          const state = await getSystemdUnitActiveState("homepi-pcm-router");
          coordinator.patchAndBroadcast(
            { pcmRouter: mapSystemdServiceStatus(state) },
            "startup-snapshot"
          );
        } catch {
          coordinator.patchAndBroadcast({ pcmRouter: "offline" }, "startup-snapshot");
        }
      },
    },
    {
      name: "nqptp",
      load: async () => {
        try {
          const state = await getSystemdUnitActiveState("homepi-nqptp");
          coordinator.patchAndBroadcast(
            { nqptp: mapSystemdServiceStatus(state) },
            "startup-snapshot"
          );
        } catch {
          coordinator.patchAndBroadcast({ nqptp: "offline" }, "startup-snapshot");
        }
      },
    },
    {
      name: "metadata",
      load: async () => {
        try {
          const state = await getSystemdUnitActiveState("homepi-metadata");
          coordinator.patchAndBroadcast(
            { metadata: mapSystemdServiceStatus(state) },
            "startup-snapshot"
          );
        } catch {
          coordinator.patchAndBroadcast({ metadata: "offline" }, "startup-snapshot");
        }
      },
    },
    {
      name: "shairport",
      load: async () => {
        try {
          const state = await getSystemdUnitActiveState("homepi-shairport-supervisor");
          coordinator.patchAndBroadcast(
            { shairport: mapSystemdServiceStatus(state) },
            "startup-snapshot"
          );
        } catch {
          coordinator.patchAndBroadcast({ shairport: "offline" }, "startup-snapshot");
        }
      },
    },
  ];
}

/**
 * Loads one-time startup snapshots for all services without failing the backend on partial errors.
 * @param loaders - Snapshot loaders to run.
 * @param logger - Structured logger.
 */
export async function loadStartupSnapshots(
  loaders: StartupSnapshotLoader[],
  logger: Logger
): Promise<void> {
  const results = await Promise.allSettled(
    loaders.map(async (loader) => {
      await loader.load();
    })
  );

  results.forEach((result, index) => {
    if (result.status === "rejected") {
      logger.warn({
        module: "app.backend.status",
        event: "startup_snapshot_failed",
        correlationId: createCorrelationId("startup-snapshot"),
        message: "Startup service snapshot failed",
        data: {
          loader: loaders[index]?.name ?? "unknown",
          error:
            result.reason instanceof Error ? result.reason.message : String(result.reason),
        },
      });
    }
  });
}
