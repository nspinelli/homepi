/**
 * Contact sensor classification.
 */
export type SensorType = "door" | "window" | "other";

/**
 * Normalized contact state.
 */
export type ContactState = "open" | "closed" | "unknown";

/**
 * One contact sensor row from the API.
 */
export interface ContactSensor {
  id: string;
  sensorNumber: number;
  name: string;
  type: SensorType;
  roomId: string | null;
  roomName: string | null;
  contactState: ContactState;
  open: boolean;
  faulted: boolean;
  faultReason: string | null;
  homekitEnabled: boolean;
  lastChangedAt: string | null;
  lastSeenAt: string | null;
}

/**
 * Service rollup for the contact sensors module.
 */
export interface SensorsServiceStatus {
  sensors: string;
  homekitBridge: string;
}

/**
 * Contact sensors module snapshot from REST.
 */
export interface SensorsSnapshot {
  module: string;
  sensorCount: number;
  sensors: ContactSensor[];
  hardwareReady: boolean;
  homekitBridgeReachable: boolean;
  services: SensorsServiceStatus;
  moduleHealth?: {
    status: string;
    userMessage?: string;
    capabilities?: Array<{ id: string; status: string; userMessage?: string }>;
  };
}

/**
 * Patch payload for sensor configuration.
 */
export interface ContactSensorPatch {
  name?: string;
  type?: SensorType;
  roomId?: string | null;
  roomName?: string | null;
  homekitEnabled?: boolean;
}
