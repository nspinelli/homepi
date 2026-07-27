/**
 * Contact sensor domain types for homepi-sensors.
 */

/** Sensor classification for UI and HomeKit. */
export type SensorType = "door" | "window" | "other";

/** Normalized contact state. */
export type ContactState = "open" | "closed" | "unknown";

/** Hardware backend for a sensor row. */
export type HardwareType = "mcp23017" | "raspberry_pi_gpio";

/**
 * Persisted contact sensor record.
 */
export interface ContactSensorRecord {
  sensorId: string;
  sensorNumber: number;
  sensorName: string;
  sensorType: SensorType;
  roomId: string | null;
  roomName: string | null;
  controllerId: string;
  hardwareType: HardwareType;
  hardwareModel: string;
  schematicNetName: string | null;
  i2cAddress: string | null;
  mcpBank: string | null;
  mcpPin: number | null;
  piPhysicalPin: number | null;
  bcmGpio: number | null;
  invertLogic: boolean;
  debounceMs: number;
  homekitEnabled: boolean;
  homekitAccessoryUuid: string | null;
  contactState: ContactState;
  rawValue: number | null;
  faulted: boolean;
  faultReason: string | null;
  tamperSupported: boolean;
  tampered: boolean;
  tamperReason: string | null;
  lastChangedAt: string | null;
  lastSeenAt: string | null;
}

/**
 * Patch payload for sensor configuration updates.
 */
export interface ContactSensorPatch {
  sensorName?: string;
  sensorType?: SensorType;
  roomId?: string | null;
  roomName?: string | null;
  homekitEnabled?: boolean;
}

/**
 * Controller seed row for MCP and Pi GPIO.
 */
export interface ControllerSeed {
  controllerId: string;
  controllerType: HardwareType;
  controllerModel: string;
  i2cBus: string | null;
  i2cAddress: string | null;
  intaNetName: string | null;
  intaPiPhysicalPin: number | null;
  intaBcmGpio: number | null;
  intbNetName: string | null;
  intbPiPhysicalPin: number | null;
  intbBcmGpio: number | null;
}

/**
 * Sensor hardware seed before runtime state.
 */
export interface SensorSeed {
  sensorNumber: number;
  controllerId: string;
  hardwareType: HardwareType;
  hardwareModel: string;
  schematicNetName: string;
  i2cAddress?: string;
  mcpBank?: string;
  mcpPin?: number;
  piPhysicalPin?: number;
  bcmGpio?: number;
}
