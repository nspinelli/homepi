import { sendCommand } from "@homepi/core-messaging";

/** Default homepi-sensors socket path. */
export const DEFAULT_SENSORS_SOCKET = "/run/homepi/sensors/sensors.sock";

/**
 * Client for the homepi-sensors module facade socket.
 */
export class SensorsClient {
  /**
   * @param socketPath - Unix socket path for homepi-sensors.
   */
  constructor(private readonly socketPath: string = DEFAULT_SENSORS_SOCKET) {}

  /**
   * Sends a command to homepi-sensors.
   * @param command - Command name.
   * @param payload - Optional payload.
   * @param correlationId - Request correlation id.
   * @returns Command result object.
   */
  async send(
    command: string,
    payload: Record<string, unknown> = {},
    _correlationId = "backend"
  ): Promise<Record<string, unknown>> {
    const response = await sendCommand(
      this.socketPath,
      "homepi-backend",
      "homepi-sensors",
      command,
      payload,
      10_000
    );

    if (!response.ok) {
      throw new Error(
        response.error?.userMessage ?? `Sensors command failed: ${command}`
      );
    }

    return (response.result ?? {}) as Record<string, unknown>;
  }

  /**
   * Fetches the contact sensors snapshot.
   * @param correlationId - Request correlation id.
   * @returns Snapshot payload.
   */
  async getSnapshot(correlationId: string): Promise<Record<string, unknown>> {
    return this.send("sensors.snapshot", {}, correlationId);
  }

  /**
   * Fetches module health from the facade.
   * @param correlationId - Request correlation id.
   * @returns Health payload.
   */
  async getHealth(correlationId: string): Promise<Record<string, unknown>> {
    return this.send("getHealth", {}, correlationId);
  }
}
