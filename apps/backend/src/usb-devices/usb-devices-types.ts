/**
 * USB device capability kind exposed by homepi-usb-devices.
 */
export type UsbDeviceKind = "serial" | "audio";

/**
 * USB device record from the native service.
 */
export interface UsbDevice {
  /** Stable device identifier. */
  deviceId: string;
  /** Human-readable label. */
  displayName: string;
  /** Device capability kind. */
  kind: UsbDeviceKind;
  /** Whether the device is currently connected. */
  present: boolean;
  /** USB vendor id. */
  idVendor?: string;
  /** USB product id. */
  idProduct?: string;
  /** USB serial when available. */
  serial?: string;
  /** udev devpath. */
  devpath?: string;
  /** ALSA card id for audio devices. */
  alsaCard?: string;
  /** Resolved hw:N,0 string. */
  resolvedAlsaName?: string;
}

/**
 * Supported PCM profile tuple for a USB audio device.
 */
export interface AudioProfileTuple {
  /** Sample rate in Hz. */
  sampleRate: number;
  /** Channel count. */
  channels: number;
  /** ALSA sample format. */
  sampleFormat: "S16_LE" | "S32_LE";
}

/**
 * Role assignments for serial and audio outputs.
 */
export interface UsbAssignments {
  /** Primary serial connection device id. */
  serial: string | null;
  /** Primary audio output device id. */
  audioPrimary: string | null;
  /** Primary paging output device id. */
  paging: string | null;
  /** User-selected PCM profile for primary audio. */
  audioPrimaryProfile?: AudioProfileTuple | null;
}

/**
 * Audio capabilities for a USB device.
 */
export interface AudioCapabilities {
  /** Device id. */
  deviceId: string;
  /** Supported profile tuples. */
  supportedProfileTuples: AudioProfileTuple[];
  /** Probe timestamp. */
  probedAt?: string;
  /** Probe error when capabilities are unavailable. */
  probeError?: string;
}

/**
 * Health snapshot from the native USB service.
 */
export interface UsbDevicesHealth {
  /** Service lifecycle state. */
  lifecycle: string;
  /** Whether udev monitor thread is active. */
  udevMonitorActive: boolean;
  /** Count of currently connected devices. */
  connectedDeviceCount: number;
  /** True when an assigned device is missing. */
  assignmentsDegraded: boolean;
  /** ISO timestamp of last scan or null. */
  lastScanAt: string | null;
}

/**
 * Socket API response envelope from homepi-usb-devices.
 */
export interface UsbSocketResponse<T = Record<string, unknown>> {
  /** Success flag. */
  ok: boolean;
  /** Correlation identifier. */
  correlationId: string;
  /** Response payload when ok. */
  data?: T;
  /** Error details when not ok. */
  error?: {
    code: string;
    message: string;
  };
}
