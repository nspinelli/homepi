import { createCorrelationId, type Logger } from "@homepi/core-logging";

import type { HifiSerialClient } from "../hifi-serial/hifi-serial-client.js";
import { getSystemdUnitActiveState } from "../runtime/check-systemd-unit.js";
import type { UsbDevicesClient } from "../usb-devices/usb-devices-client.js";
import {
  mapHifiSerialStatus,
  mapSystemdServiceStatus,
  mapUsbDevicesStatus,
} from "./service-status-mappers.js";
import type { StatusUpdateCoordinator } from "./status-update-coordinator.js";
import type { SystemStatusStore } from "../system-status-store.js";

/** Default fallback reconciliation interval (2 minutes). */
export const FALLBACK_RECONCILIATION_INTERVAL_MS = 120_000;

/**
 * Tracks whether a native event bridge is connected.
 */
export interface BridgeConnectionState {
  /** USB devices event bridge connected. */
  usbDevices: boolean;
  /** HiFi serial event bridge connected. */
  hifiSerial: boolean;
  /** PCM router event bridge connected. */
  pcmRouter: boolean;
}

/**
 * Options for slow fallback status reconciliation.
 */
export interface FallbackReconciliationOptions {
  /** Structured logger. */
  logger: Logger;
  /** Status update coordinator. */
  coordinator: StatusUpdateCoordinator;
  /** Status store for reading current values. */
  statusStore: SystemStatusStore;
  /** USB devices client for health when bridge is down. */
  usbDevicesClient: UsbDevicesClient;
  /** HiFi serial client for health when bridge is down. */
  hifiSerialClient: HifiSerialClient;
  /** Bridge connection state provider. */
  getBridgeState: () => BridgeConnectionState;
  /** Reconciliation interval in milliseconds. */
  intervalMs?: number;
}

/**
 * Slow fallback reconciliation for services without reliable event streams.
 */
export class FallbackReconciliation {
  private timer: ReturnType<typeof setInterval> | null = null;

  /**
   * @param options - Reconciliation configuration.
   */
  constructor(private readonly options: FallbackReconciliationOptions) {}

  /**
   * Starts the periodic reconciliation loop.
   */
  start(): void {
    if (this.timer) {
      return;
    }
    const intervalMs =
      this.options.intervalMs ?? FALLBACK_RECONCILIATION_INTERVAL_MS;
    this.timer = setInterval(() => {
      void this.reconcile();
    }, intervalMs);
  }

  /**
   * Stops the reconciliation loop.
   */
  stop(): void {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
  }

  private async reconcile(): Promise<void> {
    const before = this.options.statusStore.getStatus();
    const bridgeState = this.options.getBridgeState();

    await this.reconcileSystemd("nqptp", "homepi-nqptp");
    await this.reconcileMetadata();
    await this.reconcileSystemd("shairport", "homepi-shairport-supervisor");

    if (!bridgeState.usbDevices) {
      await this.reconcileUsbHealth();
    }
    if (!bridgeState.hifiSerial) {
      await this.reconcileHifiHealth();
    }
    if (!bridgeState.pcmRouter) {
      await this.reconcileSystemd("pcmRouter", "homepi-pcm-router");
    }

    const after = this.options.statusStore.getStatus();
    const changed =
      before.nqptp !== after.nqptp ||
      before.metadata !== after.metadata ||
      before.shairport !== after.shairport ||
      before.usbDevices !== after.usbDevices ||
      before.hifiSerial !== after.hifiSerial ||
      before.pcmRouter !== after.pcmRouter;

    if (changed) {
      this.options.logger.info({
        module: "app.backend.status",
        event: "fallback_reconciliation_corrected",
        correlationId: createCorrelationId("fallback-reconciliation"),
        message: "Fallback reconciliation corrected stale service status",
        data: {
          nqptp: { before: before.nqptp, after: after.nqptp },
          metadata: { before: before.metadata, after: after.metadata },
          shairport: { before: before.shairport, after: after.shairport },
          usbDevices: { before: before.usbDevices, after: after.usbDevices },
          hifiSerial: { before: before.hifiSerial, after: after.hifiSerial },
          pcmRouter: { before: before.pcmRouter, after: after.pcmRouter },
        },
      });
    }
  }

  private async reconcileSystemd(
    field: "nqptp" | "shairport" | "pcmRouter",
    unit: string
  ): Promise<void> {
    const before = this.options.statusStore.getStatus()[field];
    try {
      const state = await getSystemdUnitActiveState(unit);
      const next = mapSystemdServiceStatus(state);
      if (next !== before) {
        this.options.coordinator.patchAndBroadcast({ [field]: next }, "fallback-reconciliation");
      }
    } catch {
      if (before !== "offline") {
        this.options.coordinator.patchAndBroadcast(
          { [field]: "offline" },
          "fallback-reconciliation"
        );
      }
    }
  }

  private async reconcileMetadata(): Promise<void> {
    const before = this.options.statusStore.getStatus().metadata;
    try {
      const supervisor = await getSystemdUnitActiveState("homepi-shairport-supervisor");
      if (supervisor !== "active") {
        if (before !== "offline") {
          this.options.coordinator.patchAndBroadcast(
            { metadata: "offline" },
            "fallback-reconciliation"
          );
        }
        return;
      }
      const zone1 = await getSystemdUnitActiveState("homepi-metadata@1");
      const next = mapSystemdServiceStatus(zone1);
      if (next !== before) {
        this.options.coordinator.patchAndBroadcast(
          { metadata: next },
          "fallback-reconciliation"
        );
      }
    } catch {
      if (before !== "offline") {
        this.options.coordinator.patchAndBroadcast(
          { metadata: "offline" },
          "fallback-reconciliation"
        );
      }
    }
  }

  private async reconcileUsbHealth(): Promise<void> {
    const before = this.options.statusStore.getStatus().usbDevices;
    const correlationId = createCorrelationId("fallback-reconciliation");
    try {
      const health = await this.options.usbDevicesClient.getHealth(correlationId);
      const next = mapUsbDevicesStatus(true, health.assignmentsDegraded);
      if (next !== before) {
        this.options.coordinator.patchAndBroadcast(
          { usbDevices: next },
          "fallback-reconciliation"
        );
      }
    } catch {
      if (before !== "offline") {
        this.options.coordinator.patchAndBroadcast(
          { usbDevices: "offline" },
          "fallback-reconciliation"
        );
      }
    }
  }

  private async reconcileHifiHealth(): Promise<void> {
    const before = this.options.statusStore.getStatus().hifiSerial;
    const correlationId = createCorrelationId("fallback-reconciliation");
    try {
      const health = await this.options.hifiSerialClient.getHealth(correlationId);
      const next = mapHifiSerialStatus(health);
      if (next !== before) {
        this.options.coordinator.patchAndBroadcast(
          { hifiSerial: next },
          "fallback-reconciliation"
        );
      }
    } catch {
      if (before !== "offline") {
        this.options.coordinator.patchAndBroadcast(
          { hifiSerial: "offline" },
          "fallback-reconciliation"
        );
      }
    }
  }
}
