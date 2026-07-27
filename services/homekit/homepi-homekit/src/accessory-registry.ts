import { mkdirSync, readFileSync, writeFileSync, existsSync } from "node:fs";
import { join } from "node:path";

import {
  Accessory,
  Bridge,
  Characteristic,
  HAPStorage,
  Service,
  uuid,
} from "hap-nodejs";

/** HAP category id for bridge accessories. */
const HAP_CATEGORY_BRIDGE = 2;
/** HAP category id for sensor accessories. */
const HAP_CATEGORY_SENSOR = 10;

/**
 * Persisted accessory registration row.
 */
export interface AccessoryRegistration {
  moduleId: string;
  accessoryType: string;
  stableUuid: string;
  displayName: string;
  state: Record<string, unknown>;
  metadata?: Record<string, unknown>;
}

/**
 * Manages HAP bridged accessories and pairing identity.
 */
export class AccessoryRegistry {
  private readonly bridge: Bridge;
  private readonly accessories = new Map<string, Accessory>();
  private readonly registrations = new Map<string, AccessoryRegistration>();
  private readonly statePath: string;
  private ready = false;

  /**
   * Creates the accessory registry and HAP bridge.
   * @param stateDir - Directory for pairing and registry persistence.
   */
  constructor(stateDir: string) {
    mkdirSync(stateDir, { recursive: true });
    HAPStorage.setCustomStoragePath(stateDir);
    this.statePath = join(stateDir, "accessories.json");

    const usernamePath = join(stateDir, "hap-username.txt");
    const username = existsSync(usernamePath)
      ? readFileSync(usernamePath, "utf8").trim()
      : this.generateUsername();
    if (!existsSync(usernamePath)) {
      writeFileSync(usernamePath, username, "utf8");
    }

    this.bridge = new Bridge("HomePi", uuid.generate("HomePi-Bridge"));
    this.bridge.category = HAP_CATEGORY_BRIDGE;
    this.loadRegistrations();

    this.bridge.publish({
      username,
      pincode: "031-45-154",
      port: 51826,
    });

    this.ready = true;
  }

  /**
   * Returns whether the HAP bridge is published.
   * @returns True when ready.
   */
  isReady(): boolean {
    return this.ready;
  }

  /**
   * Registers or replaces a bridged accessory.
   * @param registration - Accessory registration payload.
   */
  register(registration: AccessoryRegistration): void {
    this.remove(registration.stableUuid);

    const accessory = new Accessory(registration.displayName, registration.stableUuid);
    accessory.category = HAP_CATEGORY_SENSOR;

    if (registration.accessoryType === "ContactSensor") {
      const service = new Service.ContactSensor(registration.displayName);
      const contactDetected = Boolean(registration.state.contactDetected);
      service
        .getCharacteristic(Characteristic.ContactSensorState)
        .updateValue(
          contactDetected
            ? Characteristic.ContactSensorState.CONTACT_NOT_DETECTED
            : Characteristic.ContactSensorState.CONTACT_DETECTED
        );
      accessory.addService(service);
    }

    this.bridge.addBridgedAccessory(accessory);
    this.accessories.set(registration.stableUuid, accessory);
    this.registrations.set(registration.stableUuid, registration);
    this.persist();
  }

  /**
   * Updates accessory display name and state.
   * @param stableUuid - Stable accessory UUID.
   * @param update - Partial update payload.
   */
  update(
    stableUuid: string,
    update: { displayName?: string; state?: Record<string, unknown> }
  ): void {
    const registration = this.registrations.get(stableUuid);
    const accessory = this.accessories.get(stableUuid);
    if (!registration || !accessory) {
      return;
    }

    if (update.displayName) {
      registration.displayName = update.displayName;
      accessory.displayName = update.displayName;
    }

    if (update.state) {
      registration.state = { ...registration.state, ...update.state };
      if (registration.accessoryType === "ContactSensor") {
        const service = accessory.getService(Service.ContactSensor);
        if (service) {
          const contactDetected = Boolean(registration.state.contactDetected);
          service
            .getCharacteristic(Characteristic.ContactSensorState)
            .updateValue(
              contactDetected
                ? Characteristic.ContactSensorState.CONTACT_NOT_DETECTED
                : Characteristic.ContactSensorState.CONTACT_DETECTED
            );
        }
      }
    }

    this.persist();
  }

  /**
   * Removes a bridged accessory.
   * @param stableUuid - Stable accessory UUID.
   */
  remove(stableUuid: string): void {
    const accessory = this.accessories.get(stableUuid);
    if (accessory) {
      this.bridge.removeBridgedAccessory(accessory);
      this.accessories.delete(stableUuid);
    }
    this.registrations.delete(stableUuid);
    this.persist();
  }

  /**
   * Lists registered accessories.
   * @returns Registration rows.
   */
  list(): AccessoryRegistration[] {
    return [...this.registrations.values()];
  }

  private loadRegistrations(): void {
    if (!existsSync(this.statePath)) {
      return;
    }
    try {
      const rows = JSON.parse(readFileSync(this.statePath, "utf8")) as AccessoryRegistration[];
      for (const row of rows) {
        this.register(row);
      }
    } catch {
      /* ignore corrupt registry */
    }
  }

  private persist(): void {
    writeFileSync(this.statePath, JSON.stringify(this.list(), null, 2), "utf8");
  }

  private generateUsername(): string {
    const bytes = Array.from({ length: 6 }, () =>
      Math.floor(Math.random() * 256)
        .toString(16)
        .padStart(2, "0")
    );
    return bytes.join(":").toUpperCase();
  }
}
