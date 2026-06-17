import { connect } from "node:net";

import type {
  AudioCapabilities,
  UsbAssignments,
  UsbDevice,
  UsbDevicesHealth,
  UsbSocketResponse,
} from "./usb-devices-types.js";

/**
 * Options for the USB devices Unix socket client.
 */
export interface UsbDevicesClientOptions {
  /** Unix socket path for homepi-usb-devices. */
  socketPath: string;
  /** Request timeout in milliseconds. */
  timeoutMs?: number;
}

/**
 * Calls the native homepi-usb-devices Unix socket API.
 */
export class UsbDevicesClient {
  private readonly socketPath: string;
  private readonly timeoutMs: number;

  /**
   * Creates a USB devices socket client.
   * @param options - Client options.
   */
  constructor(options: UsbDevicesClientOptions) {
    this.socketPath = options.socketPath;
    this.timeoutMs = options.timeoutMs ?? 30_000;
  }

  /**
   * Lists detected USB devices.
   * @param correlationId - Request correlation id.
   * @returns Device list.
   */
  async listDevices(correlationId: string): Promise<UsbDevice[]> {
    const response = await this.request<{ devices: UsbDevice[] }>("listDevices", correlationId);
    return response.devices ?? [];
  }

  /**
   * Returns saved role assignments.
   * @param correlationId - Request correlation id.
   * @returns Assignments.
   */
  async getAssignments(correlationId: string): Promise<UsbAssignments> {
    return this.request<UsbAssignments>("getAssignments", correlationId);
  }

  /**
   * Persists role assignments.
   * @param assignments - Assignments to save.
   * @param correlationId - Request correlation id.
   * @returns Saved assignments.
   */
  async setAssignments(
    assignments: UsbAssignments,
    correlationId: string
  ): Promise<UsbAssignments> {
    return this.request<UsbAssignments>("setAssignments", correlationId, { assignments });
  }

  /**
   * Returns native service health.
   * @param correlationId - Request correlation id.
   * @returns Health snapshot.
   */
  async getHealth(correlationId: string): Promise<UsbDevicesHealth> {
    return this.request<UsbDevicesHealth>("getHealth", correlationId);
  }

  /**
   * Returns probed audio capabilities for a device.
   * @param deviceId - USB device id.
   * @param correlationId - Request correlation id.
   * @returns Capabilities payload.
   */
  async getAudioCapabilities(
    deviceId: string,
    correlationId: string
  ): Promise<AudioCapabilities> {
    return this.request<AudioCapabilities>("getAudioCapabilities", correlationId, { deviceId });
  }

  /**
   * Returns the active operating profile artifact view.
   * @param correlationId - Request correlation id.
   * @returns Operating profile JSON object.
   */
  async getOperatingProfile(
    correlationId: string
  ): Promise<Record<string, unknown>> {
    return this.request<Record<string, unknown>>("getOperatingProfile", correlationId);
  }

  /**
   * Probes whether the socket is reachable.
   * @param correlationId - Request correlation id.
   * @returns True when health call succeeds.
   */
  async isReachable(correlationId: string): Promise<boolean> {
    try {
      await this.getHealth(correlationId);
      return true;
    } catch {
      return false;
    }
  }

  /**
   * Sends a request to the Unix socket API.
   * @param method - Socket method name.
   * @param correlationId - Correlation id.
   * @param body - Optional extra fields.
   * @returns Parsed data payload.
   */
  private request<T>(
    method: string,
    correlationId: string,
    body: Record<string, unknown> = {}
  ): Promise<T> {
    const payload = JSON.stringify({ method, correlationId, ...body });

    return new Promise<T>((resolve, reject) => {
      const socket = connect(this.socketPath);
      let buffer = "";
      let settled = false;

      const timer = setTimeout(() => {
        if (!settled) {
          settled = true;
          socket.destroy();
          reject(new Error(`USB devices socket timeout: ${method}`));
        }
      }, this.timeoutMs);

      const finish = (error?: Error, data?: T): void => {
        if (settled) {
          return;
        }
        settled = true;
        clearTimeout(timer);
        socket.destroy();
        if (error) {
          reject(error);
          return;
        }
        resolve(data as T);
      };

      socket.on("error", (error) => finish(error));
      socket.on("data", (chunk) => {
        buffer += chunk.toString("utf8");
        if (!buffer.includes("\n")) {
          return;
        }
        try {
          const parsed = JSON.parse(buffer.trim()) as UsbSocketResponse<T>;
          if (!parsed.ok) {
            finish(new Error(parsed.error?.message ?? "USB devices request failed"));
            return;
          }
          finish(undefined, parsed.data as T);
        } catch (error) {
          finish(error instanceof Error ? error : new Error("Invalid USB socket response"));
        }
      });

      socket.on("connect", () => {
        socket.write(`${payload}\n`);
      });
    });
  }
}
