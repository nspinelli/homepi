/**
 * Canonical Unix socket paths under /run/homepi for v2 architecture.
 */
export interface RuntimeSocketPaths {
  /** Hi-Fi serial command socket. */
  hifiSerial: string;
  /** PCM router command socket. */
  pcmRouter: string;
  /** Metadata command socket. */
  metadata: string;
  /** Audio realtime progress socket. */
  audioRealtime: string;
  /** Audio paging command socket. */
  paging: string;
  /** USB devices command socket. */
  usbDevices: string;
  /** Event broker socket. */
  broker: string;
  /** Health observer socket. */
  health: string;
}

/**
 * Resolves canonical runtime socket paths from the configured socket directory.
 * @param socketDir - Base runtime directory (typically `/run/homepi`).
 * @returns Canonical socket paths for backend clients and bridges.
 */
export function resolveRuntimeSocketPaths(socketDir: string): RuntimeSocketPaths {
  return {
    hifiSerial: `${socketDir}/audio/hifi-serial.sock`,
    pcmRouter: `${socketDir}/audio/pcm-router.sock`,
    metadata: `${socketDir}/audio/metadata.sock`,
    audioRealtime: `${socketDir}/audio/audio-realtime.sock`,
    paging: `${socketDir}/audio/paging.sock`,
    usbDevices: `${socketDir}/usb/usb.sock`,
    broker: `${socketDir}/broker/broker.sock`,
    health: `${socketDir}/health/health.sock`,
  };
}
