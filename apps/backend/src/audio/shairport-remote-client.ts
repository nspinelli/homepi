import mqtt, { type MqttClient } from "mqtt";

/** Shairport MQTT remote commands accepted by the backend API. */
export const SHAIRPORT_REMOTE_COMMANDS = [
  "play",
  "pause",
  "playpause",
  "playresume",
  "stop",
  "nextitem",
  "previtem",
  "volumedown",
  "volumeup",
  "mutetoggle",
  "shuffle_songs",
] as const;

/** Valid Shairport remote command name. */
export type ShairportRemoteCommand = (typeof SHAIRPORT_REMOTE_COMMANDS)[number];

/**
 * Options for the Shairport MQTT remote client.
 */
export interface ShairportRemoteClientOptions {
  /** MQTT broker hostname. */
  host?: string;
  /** MQTT broker port. */
  port?: number;
}

/**
 * Publishes Shairport Sync remote-control commands and fetches cover art over MQTT.
 */
export class ShairportRemoteClient {
  private readonly brokerUrl: string;
  private client: MqttClient | null = null;
  private connectPromise: Promise<MqttClient> | null = null;

  /**
   * Creates a Shairport MQTT client.
   * @param options - Broker connection options.
   */
  constructor(options: ShairportRemoteClientOptions = {}) {
    const host = options.host ?? "127.0.0.1";
    const port = options.port ?? 1883;
    this.brokerUrl = `mqtt://${host}:${port}`;
  }

  /**
   * Returns true when the command is an allowed Shairport remote command.
   * @param command - Command string from the API request.
   * @returns Whether the command is supported.
   */
  isAllowedCommand(command: string): command is ShairportRemoteCommand {
    return (SHAIRPORT_REMOTE_COMMANDS as readonly string[]).includes(command);
  }

  /**
   * Publishes a remote-control command to a Shairport zone topic.
   * @param zoneId - Zone number 1–16.
   * @param command - Shairport remote command.
   */
  async publishRemoteCommand(zoneId: number, command: ShairportRemoteCommand): Promise<void> {
    const client = await this.getClient();
    const topic = `shairport/zone/${zoneId}/remote`;

    await new Promise<void>((resolve, reject) => {
      client.publish(topic, command, { qos: 0 }, (error) => {
        if (error) {
          reject(error);
          return;
        }
        resolve();
      });
    });
  }

  /**
   * Reads the latest retained MQTT payload for a Shairport zone field.
   * @param zoneId - Zone number 1–16.
   * @param field - Topic suffix (e.g. playing, volume).
   * @returns Payload string or null when unavailable.
   */
  async fetchRetainedTopic(zoneId: number, field: string): Promise<string | null> {
    const topic = `shairport/zone/${zoneId}/${field}`;

    return new Promise<string | null>((resolve, reject) => {
      const subscriber = mqtt.connect(this.brokerUrl, {
        clientId: `homepi-backend-retained-${zoneId}-${field}-${Date.now()}`,
        protocolVersion: 4,
      });

      const timeout = setTimeout(() => {
        cleanup();
        resolve(null);
      }, 1_500);

      const cleanup = (): void => {
        clearTimeout(timeout);
        subscriber.removeAllListeners();
        subscriber.end(true);
      };

      subscriber.on("error", (error) => {
        cleanup();
        reject(error);
      });

      subscriber.on("connect", () => {
        subscriber.subscribe(topic, { qos: 0 }, (error) => {
          if (error) {
            cleanup();
            reject(error);
          }
        });
      });

      subscriber.on("message", (messageTopic, payload) => {
        if (messageTopic !== topic) {
          return;
        }
        cleanup();
        const value = payload.toString("utf8");
        resolve(value.length > 0 && value !== "--" ? value : null);
      });
    });
  }

  /**
   * Fetches retained album cover art for a zone from MQTT.
   * @param zoneId - Zone number 1–16.
   * @returns Cover image bytes or null when unavailable.
   */
  async fetchCoverArt(zoneId: number): Promise<Buffer | null> {
    const topic = `shairport/zone/${zoneId}/cover`;

    return new Promise<Buffer | null>((resolve, reject) => {
      const subscriber = mqtt.connect(this.brokerUrl, {
        clientId: `homepi-backend-cover-${zoneId}-${Date.now()}`,
        protocolVersion: 4,
      });

      const timeout = setTimeout(() => {
        cleanup();
        resolve(null);
      }, 3_000);

      const cleanup = (): void => {
        clearTimeout(timeout);
        subscriber.removeAllListeners();
        subscriber.end(true);
      };

      subscriber.on("error", (error) => {
        cleanup();
        reject(error);
      });

      subscriber.on("connect", () => {
        subscriber.subscribe(topic, { qos: 0 }, (error) => {
          if (error) {
            cleanup();
            reject(error);
          }
        });
      });

      subscriber.on("message", (messageTopic, payload) => {
        if (messageTopic !== topic || payload.length === 0 || payload.toString() === "--") {
          return;
        }
        cleanup();
        resolve(Buffer.from(payload));
      });
    });
  }

  /**
   * Closes the persistent MQTT connection.
   */
  async close(): Promise<void> {
    if (!this.client) {
      return;
    }
    const client = this.client;
    this.client = null;
    this.connectPromise = null;
    await new Promise<void>((resolve) => {
      client.end(false, {}, () => resolve());
    });
  }

  private getClient(): Promise<MqttClient> {
    if (this.client?.connected) {
      return Promise.resolve(this.client);
    }
    if (this.connectPromise) {
      return this.connectPromise;
    }

    this.connectPromise = new Promise<MqttClient>((resolve, reject) => {
      const client = mqtt.connect(this.brokerUrl, {
        clientId: `homepi-backend-remote-${Date.now()}`,
        protocolVersion: 4,
        reconnectPeriod: 5_000,
      });

      const onError = (error: Error): void => {
        client.removeListener("connect", onConnect);
        this.connectPromise = null;
        reject(error);
      };

      const onConnect = (): void => {
        client.removeListener("error", onError);
        this.client = client;
        resolve(client);
      };

      client.once("connect", onConnect);
      client.once("error", onError);
    });

    return this.connectPromise;
  }
}
