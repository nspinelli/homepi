import { connect } from "node:net";

import type { HifiSerialHealth, HifiSocketResponse } from "./hifi-serial-types.js";

/**
 * Options for the HiFi serial Unix socket client.
 */
export interface HifiSerialClientOptions {
  /** Unix socket path for homepi-hifi-serial. */
  socketPath: string;
  /** Request timeout in milliseconds. */
  timeoutMs?: number;
}

/**
 * Calls the native homepi-hifi-serial Unix socket API.
 */
export class HifiSerialClient {
  private readonly socketPath: string;
  private readonly timeoutMs: number;

  /**
   * Creates a HiFi serial socket client.
   * @param options - Client options.
   */
  constructor(options: HifiSerialClientOptions) {
    this.socketPath = options.socketPath;
    this.timeoutMs = options.timeoutMs ?? 10_000;
  }

  /**
   * Returns native service health.
   * @param correlationId - Request correlation id.
   * @returns Health snapshot.
   */
  async getHealth(correlationId: string): Promise<HifiSerialHealth> {
    return this.request<HifiSerialHealth>("getHealth", correlationId);
  }

  /**
   * Triggers a full controller sync.
   * @param correlationId - Request correlation id.
   */
  async syncController(correlationId: string): Promise<{ synced: boolean }> {
    return this.request<{ synced: boolean }>("syncController", correlationId);
  }

  /**
   * Returns full cached snapshot from the database.
   * @param correlationId - Request correlation id.
   * @returns Snapshot payload.
   */
  async getSnapshot(correlationId: string): Promise<Record<string, unknown>> {
    return this.request<Record<string, unknown>>("getSnapshot", correlationId);
  }

  /**
   * Returns controller state.
   * @param correlationId - Request correlation id.
   * @returns Controller record.
   */
  async getController(correlationId: string): Promise<Record<string, unknown>> {
    return this.request<Record<string, unknown>>("getController", correlationId);
  }

  /**
   * Returns all zones.
   * @param correlationId - Request correlation id.
   * @returns Zones list wrapper.
   */
  async getZones(correlationId: string): Promise<{ zones: unknown[] }> {
    return this.request<{ zones: unknown[] }>("getZones", correlationId);
  }

  /**
   * Returns all sources.
   * @param correlationId - Request correlation id.
   * @returns Sources list wrapper.
   */
  async getSources(correlationId: string): Promise<{ sources: unknown[] }> {
    return this.request<{ sources: unknown[] }>("getSources", correlationId);
  }

  /**
   * Returns all groups.
   * @param correlationId - Request correlation id.
   * @returns Groups list wrapper.
   */
  async getGroups(correlationId: string): Promise<{ groups: unknown[] }> {
    return this.request<{ groups: unknown[] }>("getGroups", correlationId);
  }

  /**
   * Returns language strings.
   * @param correlationId - Request correlation id.
   * @returns Language strings wrapper.
   */
  async getLanguageStrings(
    correlationId: string
  ): Promise<{ languageStrings: unknown[] }> {
    return this.request<{ languageStrings: unknown[] }>("getLanguageStrings", correlationId);
  }

  /**
   * Sends a raw protocol command to the controller queue.
   * @param command - Command string (e.g. *Z1VOLUME50).
   * @param correlationId - Request correlation id.
   */
  async sendCommand(command: string, correlationId: string): Promise<{ queued: boolean }> {
    return this.request<{ queued: boolean }>("sendCommand", correlationId, { command });
  }

  /**
   * Updates cached controller zone fields in SQLite (does not send serial commands).
   * @param zoneNumber - Zone 1-16.
   * @param fields - Controller fields to persist.
   * @param correlationId - Request correlation id.
   */
  async patchZoneController(
    zoneNumber: number,
    fields: Record<string, unknown>,
    correlationId: string
  ): Promise<{ patched: boolean }> {
    return this.request<{ patched: boolean }>("patchZoneController", correlationId, {
      zoneNumber,
      ...fields,
    });
  }

  /**
   * Updates cached controller source fields in SQLite (does not send serial commands).
   * @param sourceNumber - Source 1-8.
   * @param fields - Source fields to persist.
   * @param correlationId - Request correlation id.
   */
  async patchSource(
    sourceNumber: number,
    fields: Record<string, unknown>,
    correlationId: string
  ): Promise<{ patched: boolean }> {
    return this.request<{ patched: boolean }>("patchSource", correlationId, {
      sourceNumber,
      ...fields,
    });
  }

  /**
   * Returns the configured AirPlay source number.
   * @param correlationId - Request correlation id.
   */
  async getAirplaySource(
    correlationId: string
  ): Promise<{ sourceNumber: number | null }> {
    return this.request<{ sourceNumber: number | null }>("getAirplaySource", correlationId);
  }

  /**
   * Sets the AirPlay source designation.
   * @param sourceNumber - Source slot 1-8.
   * @param correlationId - Request correlation id.
   */
  async setAirplaySource(
    sourceNumber: number,
    correlationId: string
  ): Promise<{ sourceNumber: number }> {
    return this.request<{ sourceNumber: number }>("setAirplaySource", correlationId, {
      sourceNumber,
    });
  }

  /**
   * Returns all Shairport per-zone settings rows.
   * @param correlationId - Request correlation id.
   * @returns Shairport zone settings list.
   */
  async getShairportZoneSettings(
    correlationId: string
  ): Promise<{ shairportZoneSettings: unknown[] }> {
    return this.request<{ shairportZoneSettings: unknown[] }>(
      "getShairportZoneSettings",
      correlationId
    );
  }

  /**
   * Updates Shairport settings for one zone in SQLite.
   * @param zoneNumber - Zone 1-16.
   * @param settings - Settings to update.
   * @param correlationId - Request correlation id.
   */
  async updateShairportZoneSettings(
    zoneNumber: number,
    settings: {
      volumeControlProfile?: string;
      activeStateTimeout?: number;
      sessionTimeout?: number;
      logVerbosity?: number;
    },
    correlationId: string
  ): Promise<{ updated: boolean }> {
    return this.request<{ updated: boolean }>("updateShairportZoneSettings", correlationId, {
      zoneNumber,
      ...settings,
    });
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
          reject(new Error(`HiFi serial socket timeout: ${method}`));
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
        const lines = buffer.split("\n");
        for (const line of lines) {
          if (!line.trim()) {
            continue;
          }
          try {
            const parsed = JSON.parse(line) as HifiSocketResponse<T> & { event?: string };
            if (parsed.event) {
              continue;
            }
            if (!parsed.ok) {
              finish(new Error(parsed.error?.message ?? "HiFi serial request failed"));
              return;
            }
            finish(undefined, parsed.data as T);
            return;
          } catch {
            continue;
          }
        }
        buffer = lines.at(-1) ?? "";
      });

      socket.on("connect", () => {
        socket.write(`${payload}\n`);
      });
    });
  }
}
