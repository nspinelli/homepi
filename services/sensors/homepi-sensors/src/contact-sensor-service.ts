import type { Logger } from "@homepi/core-logging";

import { CONTACT_SENSOR_CONTROLLERS } from "./config/sensor-map.js";
import { BrokerPublisher } from "./broker/broker-publisher.js";
import {
  type SensorRepository,
  serializeSensorRecord,
} from "./db/sensor-repository.js";
import { ContactStateNormalizer } from "./hardware/contact-state-normalizer.js";
import { GpioLineWatcher, type GpioWatchLine } from "./hardware/gpio-line-watcher.js";
import {
  Mcp23017Manager,
  parseI2cAddress,
  type McpConfig,
} from "./hardware/mcp23017-manager.js";
import { HomekitAccessoryClient } from "./homekit/homekit-accessory-client.js";
import type {
  ContactSensorPatch,
  ContactSensorRecord,
  ContactState,
} from "./types/contact-sensor-types.js";

/**
 * Options for the contact sensor runtime service.
 */
export interface ContactSensorServiceOptions {
  /** Sensor repository. */
  repository: SensorRepository;
  /** Structured logger. */
  logger: Logger;
  /** Broker publisher. */
  broker: BrokerPublisher;
  /** HomeKit bridge client. */
  homekit: HomekitAccessoryClient;
  /** GPIO chip device for header pins. */
  gpioChip: string;
}

/**
 * Orchestrates hardware, persistence, broker, and HomeKit for contact sensors.
 */
export class ContactSensorService {
  private readonly repository: SensorRepository;
  private readonly logger: Logger;
  private readonly broker: BrokerPublisher;
  private readonly homekit: HomekitAccessoryClient;
  private readonly gpioChip: string;
  private readonly normalizer = new ContactStateNormalizer();
  private readonly gpioWatcher: GpioLineWatcher;
  private readonly mcpManager: Mcp23017Manager;
  private hardwareReady = false;
  private homekitReachable = false;
  private readonly sensorById = new Map<string, ContactSensorRecord>();
  private readonly mcpSensorIndex = new Map<string, ContactSensorRecord[]>();

  /**
   * Creates the contact sensor service.
   * @param options - Service dependencies.
   */
  constructor(options: ContactSensorServiceOptions) {
    this.repository = options.repository;
    this.logger = options.logger;
    this.broker = options.broker;
    this.homekit = options.homekit;
    this.gpioChip = options.gpioChip;
    this.gpioWatcher = new GpioLineWatcher(options.logger);

    const mcpConfigs: McpConfig[] = CONTACT_SENSOR_CONTROLLERS.filter(
      (c) => c.controllerType === "mcp23017"
    ).map((c) => ({
      controllerId: c.controllerId,
      busNumber: 1,
      address: parseI2cAddress(c.i2cAddress ?? "0x20"),
      intaBcmGpio: c.intaBcmGpio ?? 0,
      intbBcmGpio: c.intbBcmGpio ?? 0,
    }));

    this.mcpManager = new Mcp23017Manager(mcpConfigs, options.logger);
    this.rebuildIndexes();
  }

  /**
   * Starts hardware watchers and performs initial state read.
   */
  async start(): Promise<void> {
    this.homekitReachable = await this.homekit.isReachable();
    await this.mcpManager.initialize();

    if (this.mcpManager.isHealthy()) {
      await this.bootstrapMcpStates();
      this.startInterruptWatches();
      this.hardwareReady = true;
    } else {
      const reason = this.mcpManager.getFaultReason() ?? "MCP unavailable";
      for (const controller of CONTACT_SENSOR_CONTROLLERS.filter(
        (c) => c.controllerType === "mcp23017"
      )) {
        this.repository.markControllerFaulted(controller.controllerId, reason);
      }
      this.rebuildIndexes();
      this.logger.warn({
        module: "sensors",
        event: "hardware_degraded",
        message: reason,
      });
    }

    await this.bootstrapDirectGpioStates();
    this.startDirectGpioWatches();
  }

  /**
   * Stops GPIO watchers and closes I2C.
   */
  async stop(): Promise<void> {
    this.normalizer.clear();
    this.gpioWatcher.stop();
    await this.mcpManager.close();
  }

  /**
   * Returns module health for homepi-health probes.
   * @returns Health payload.
   */
  getHealth(): Record<string, unknown> {
    const mcpHealthy = this.mcpManager.isHealthy();
    const detectionStatus = mcpHealthy ? "healthy" : this.hardwareReady ? "degraded" : "offline";
    return {
      module: "contact-sensors",
      status: mcpHealthy ? "healthy" : "degraded",
      userMessage: mcpHealthy
        ? undefined
        : this.mcpManager.getFaultReason() ?? "Contact sensor hardware is unavailable.",
      capabilities: [
        { id: "contact-detection", status: detectionStatus },
        { id: "tamper-fault", status: "healthy" },
        {
          id: "homekit-bridge",
          status: this.homekitReachable ? "healthy" : "offline",
          userMessage: this.homekitReachable
            ? undefined
            : "HomeKit bridge is offline, but HomePi modules are still running.",
        },
      ],
    };
  }

  /**
   * Returns a full sensors snapshot.
   * @returns Snapshot payload.
   */
  getSnapshot(): Record<string, unknown> {
    const sensors = this.repository.listSensors().map(serializeSensorRecord);
    return {
      module: "contact-sensors",
      sensorCount: sensors.length,
      sensors,
      hardwareReady: this.hardwareReady,
      homekitBridgeReachable: this.homekitReachable,
    };
  }

  /**
   * Returns one sensor with diagnostics.
   * @param sensorId - Sensor id.
   * @returns Sensor payload or null.
   */
  getSensor(sensorId: string): Record<string, unknown> | null {
    const sensor = this.repository.getSensor(sensorId);
    if (!sensor) {
      return null;
    }
    return {
      sensor: serializeSensorRecord(sensor),
      diagnostics: buildDiagnostics(sensor),
    };
  }

  /**
   * Returns module diagnostics rollup.
   * @returns Diagnostics payload.
   */
  getDiagnostics(): Record<string, unknown> {
    const sensors = this.repository.listSensors();
    return {
      hardwareReady: this.hardwareReady,
      mcpHealthy: this.mcpManager.isHealthy(),
      mcpFaultReason: this.mcpManager.getFaultReason(),
      homekitBridgeReachable: this.homekitReachable,
      sensorCount: sensors.length,
      openCount: sensors.filter((s) => s.contactState === "open").length,
      closedCount: sensors.filter((s) => s.contactState === "closed").length,
      unknownCount: sensors.filter((s) => s.contactState === "unknown").length,
      faultedCount: sensors.filter((s) => s.faulted).length,
    };
  }

  /**
   * Patches sensor configuration and syncs HomeKit when enabled.
   * @param sensorId - Sensor id.
   * @param patch - Configuration patch.
   * @returns Updated sensor or null.
   */
  async patchSensor(
    sensorId: string,
    patch: ContactSensorPatch
  ): Promise<ContactSensorRecord | null> {
    const updated = this.repository.patchSensor(sensorId, patch);
    if (!updated) {
      return null;
    }

    this.sensorById.set(sensorId, updated);
    await this.broker.publishConfigUpdated(updated);

    if (this.homekitReachable) {
      await this.homekit.syncContactSensor(updated);
    }

    return updated;
  }

  private rebuildIndexes(): void {
    this.sensorById.clear();
    this.mcpSensorIndex.clear();

    for (const sensor of this.repository.listSensors()) {
      this.sensorById.set(sensor.sensorId, sensor);
      if (sensor.hardwareType !== "mcp23017" || !sensor.mcpBank) {
        continue;
      }
      const key = `${sensor.controllerId}:${sensor.mcpBank}`;
      const list = this.mcpSensorIndex.get(key) ?? [];
      list.push(sensor);
      this.mcpSensorIndex.set(key, list);
    }
  }

  private async bootstrapMcpStates(): Promise<void> {
    for (const config of this.mcpManager.getConfigs()) {
      try {
        const { bankA, bankB } = await this.mcpManager.readBanks(config);
        this.applyMcpBank(config.controllerId, "A", bankA);
        this.applyMcpBank(config.controllerId, "B", bankB);
      } catch (error) {
        const reason =
          error instanceof Error ? error.message : "Failed to read MCP banks";
        this.repository.markControllerFaulted(config.controllerId, reason);
      }
    }
    this.rebuildIndexes();
  }

  private async bootstrapDirectGpioStates(): Promise<void> {
    for (const sensor of this.repository.listSensors()) {
      if (sensor.hardwareType !== "raspberry_pi_gpio" || sensor.bcmGpio === null) {
        continue;
      }
      const raw = await this.gpioWatcher.readLine(this.gpioChip, sensor.bcmGpio);
      if (raw === null) {
        this.commitState(sensor.sensorId, "unknown", 0, true, "GPIO read failed");
        continue;
      }
      const state = this.normalizer.normalizeImmediate(raw === 1, sensor.invertLogic);
      this.commitState(sensor.sensorId, state, raw, false, null);
    }
    this.rebuildIndexes();
  }

  private startInterruptWatches(): void {
    const lines: GpioWatchLine[] = [];
    for (const config of this.mcpManager.getConfigs()) {
      lines.push({
        key: `${config.controllerId}-inta`,
        chip: this.gpioChip,
        offset: config.intaBcmGpio,
        controllerId: config.controllerId,
        mcpBank: "A",
      });
      lines.push({
        key: `${config.controllerId}-intb`,
        chip: this.gpioChip,
        offset: config.intbBcmGpio,
        controllerId: config.controllerId,
        mcpBank: "B",
      });
    }

    this.gpioWatcher.watch(lines, (line) => {
      if (!line.controllerId || !line.mcpBank) {
        return;
      }
      void this.mcpManager.handleInterrupt(line.controllerId, line.mcpBank, (controllerId, bank, value) => {
        this.applyMcpBank(controllerId, bank, value);
      });
    });
  }

  private startDirectGpioWatches(): void {
    const lines: GpioWatchLine[] = this.repository
      .listSensors()
      .filter((s) => s.hardwareType === "raspberry_pi_gpio" && s.bcmGpio !== null)
      .map((sensor) => ({
        key: sensor.sensorId,
        chip: this.gpioChip,
        offset: sensor.bcmGpio!,
        sensorId: sensor.sensorId,
      }));

    this.gpioWatcher.watch(lines, (line) => {
      if (!line.sensorId) {
        return;
      }
      void this.handleDirectGpioEdge(line.sensorId);
    });
  }

  private async handleDirectGpioEdge(sensorId: string): Promise<void> {
    const sensor = this.repository.getSensor(sensorId);
    if (!sensor || sensor.bcmGpio === null) {
      return;
    }

    const raw = await this.gpioWatcher.readLine(this.gpioChip, sensor.bcmGpio);
    if (raw === null) {
      return;
    }

    this.normalizer.scheduleDebounce(
      sensorId,
      sensor.debounceMs,
      () => this.normalizer.normalizeImmediate(raw === 1, sensor.invertLogic),
      (state) => {
        this.commitState(sensorId, state, raw, false, null);
      }
    );
  }

  private applyMcpBank(controllerId: string, bank: "A" | "B", capturedValue: number): void {
    const key = `${controllerId}:${bank}`;
    const sensors = this.mcpSensorIndex.get(key) ?? [];

    for (const sensor of sensors) {
      if (sensor.mcpPin === null) {
        continue;
      }
      const bit = (capturedValue >> sensor.mcpPin) & 1;
      const rawHigh = bit === 1;

      this.normalizer.scheduleDebounce(
        sensor.sensorId,
        sensor.debounceMs,
        () => this.normalizer.normalizeImmediate(rawHigh, sensor.invertLogic),
        (state) => {
          this.commitState(sensor.sensorId, state, bit, false, null);
        }
      );
    }
  }

  private commitState(
    sensorId: string,
    state: ContactState,
    rawValue: number,
    faulted: boolean,
    faultReason: string | null
  ): void {
    const prior = this.repository.getSensor(sensorId);
    if (!prior) {
      return;
    }

    this.repository.updateSensorState(sensorId, state, rawValue, faulted, faultReason);
    const updated = this.repository.getSensor(sensorId);
    if (!updated) {
      return;
    }

    const stateChanged = prior.contactState !== updated.contactState;
    const faultChanged = prior.faulted !== updated.faulted;

    this.sensorById.set(sensorId, updated);

    if (stateChanged) {
      void this.broker.publishContactChanged(updated);
      if (this.homekitReachable && updated.homekitEnabled) {
        void this.homekit.updateContactSensor(updated);
      }
    }

    if (faultChanged && updated.faulted) {
      void this.broker.publishFaulted(updated);
    }
  }
}

/**
 * Builds per-sensor diagnostics object.
 * @param sensor - Sensor record.
 * @returns Diagnostics payload.
 */
function buildDiagnostics(sensor: ContactSensorRecord): Record<string, unknown> {
  return {
    sensorId: sensor.sensorId,
    hardwareType: sensor.hardwareType,
    controllerId: sensor.controllerId,
    bcmGpio: sensor.bcmGpio,
    mcpBank: sensor.mcpBank,
    mcpPin: sensor.mcpPin,
    rawValue: sensor.rawValue,
    debounceMs: sensor.debounceMs,
    invertLogic: sensor.invertLogic,
    lastChangedAt: sensor.lastChangedAt,
    lastSeenAt: sensor.lastSeenAt,
  };
}
