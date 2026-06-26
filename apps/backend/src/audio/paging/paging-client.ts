import { connect } from "node:net";

import type {
  PagingApiKeyMetadata,
  PagingChime,
  PagingConfig,
  PagingConfigUpdate,
  PagingSocketResponse,
  PagingStatus,
  PagingVoice,
} from "./paging-types.js";

/**
 * Options for the paging Unix socket client.
 */
export interface PagingClientOptions {
  /** Unix socket path for homepi-audio-paging. */
  socketPath: string;
  /** Request timeout in milliseconds. */
  timeoutMs?: number;
}

/**
 * Request payload for installing a voice.
 */
export interface PagingInstallVoiceRequest {
  /** Voice identifier to install. */
  voiceId: string;
  /** Optional flag to set the voice as default after install. */
  setDefault?: boolean;
  /** Friendly display name stored in paging DB. */
  displayName?: string;
  /** BCP-47 language code stored in paging DB. */
  languageCode?: string;
  /** Voice quality tier stored in paging DB. */
  quality?: string;
  /** Absolute ONNX model path on disk. */
  modelPath?: string;
  /** Absolute JSON config path on disk. */
  configPath?: string;
  /** True when the voice ships with HomePi. */
  isBundled?: boolean;
}

/**
 * Request payload for uploading a custom chime.
 */
export interface PagingUploadChimeRequest {
  /** Stable chime identifier. */
  chimeId: string;
  /** Display name for the uploaded chime. */
  displayName: string;
  /** Absolute path to the stored WAV file. */
  filePath: string;
  /** Duration in milliseconds. */
  durationMs: number;
}

/**
 * Unix socket client for homepi-audio-paging.
 */
export class PagingClient {
  private readonly socketPath: string;
  private readonly timeoutMs: number;

  /**
   * Creates a paging socket client.
   * @param options - Client options.
   */
  constructor(options: PagingClientOptions) {
    this.socketPath = options.socketPath;
    this.timeoutMs = options.timeoutMs ?? 10_000;
  }

  /**
   * Returns current paging configuration.
   * @param correlationId - Request correlation id.
   * @returns Paging configuration payload.
   */
  async getConfig(correlationId: string): Promise<PagingConfig> {
    return this.request<PagingConfig>("getConfig", correlationId);
  }

  /**
   * Persists paging configuration updates.
   * @param update - Mutable config fields.
   * @param correlationId - Request correlation id.
   * @returns Updated paging configuration payload.
   */
  async updateConfig(update: PagingConfigUpdate, correlationId: string): Promise<PagingConfig> {
    return this.request<PagingConfig>("updateConfig", correlationId, update);
  }

  /**
   * Returns current paging runtime status.
   * @param correlationId - Request correlation id.
   * @returns Paging status snapshot.
   */
  async getStatus(correlationId: string): Promise<PagingStatus> {
    return this.request<PagingStatus>("getStatus", correlationId);
  }

  /**
   * Returns locally installed voices.
   * @param correlationId - Request correlation id.
   * @returns Installed voice metadata.
   */
  async getVoices(correlationId: string): Promise<{ voices: PagingVoice[] }> {
    return this.request<{ voices: PagingVoice[] }>("getVoices", correlationId);
  }

  /**
   * Returns chime metadata rows.
   * @param correlationId - Request correlation id.
   * @returns Chime metadata list.
   */
  async getChimes(correlationId: string): Promise<{ chimes: PagingChime[] }> {
    return this.request<{ chimes: PagingChime[] }>("getChimes", correlationId);
  }

  /**
   * Installs a voice and optionally sets it as default.
   * @param request - Voice install request.
   * @param correlationId - Request correlation id.
   * @returns Install result payload.
   */
  async installVoice(
    request: PagingInstallVoiceRequest,
    correlationId: string
  ): Promise<Record<string, unknown>> {
    return this.request<Record<string, unknown>>(
      "installVoice",
      correlationId,
      request as unknown as Record<string, unknown>
    );
  }

  /**
   * Removes an installed voice.
   * @param voiceId - Voice identifier to remove.
   * @param correlationId - Request correlation id.
   * @returns Removal result payload.
   */
  async removeVoice(voiceId: string, correlationId: string): Promise<Record<string, unknown>> {
    return this.request<Record<string, unknown>>("removeVoice", correlationId, { voiceId });
  }

  /**
   * Reloads the Piper worker for a voice change.
   * @param voiceId - Voice identifier to load.
   * @param correlationId - Request correlation id.
   * @returns Reload result payload.
   */
  async reloadVoice(voiceId: string, correlationId: string): Promise<Record<string, unknown>> {
    return this.request<Record<string, unknown>>("reloadVoice", correlationId, { voiceId });
  }

  /**
   * Uploads a new custom chime.
   * @param request - Chime upload request.
   * @param correlationId - Request correlation id.
   * @returns Upload result payload.
   */
  async uploadChime(
    request: PagingUploadChimeRequest,
    correlationId: string
  ): Promise<Record<string, unknown>> {
    return this.request<Record<string, unknown>>(
      "uploadChime",
      correlationId,
      request as unknown as Record<string, unknown>
    );
  }

  /**
   * Removes a chime by id.
   * @param chimeId - Chime identifier to remove.
   * @param correlationId - Request correlation id.
   * @returns Removal result payload.
   */
  async removeChime(chimeId: string, correlationId: string): Promise<Record<string, unknown>> {
    return this.request<Record<string, unknown>>("removeChime", correlationId, { chimeId });
  }

  /**
   * Updates active chime selection.
   * @param chimeId - Chime identifier to activate.
   * @param correlationId - Request correlation id.
   * @returns Update result payload.
   */
  async setActiveChime(chimeId: string, correlationId: string): Promise<Record<string, unknown>> {
    return this.request<Record<string, unknown>>("setActiveChime", correlationId, { chimeId });
  }

  /**
   * Reads paging API key metadata from the paging service.
   * @param correlationId - Request correlation id.
   * @returns Key metadata including prefix and hash.
   */
  async getApiKey(correlationId: string): Promise<PagingApiKeyMetadata> {
    return this.request<PagingApiKeyMetadata>("getApiKey", correlationId);
  }

  /**
   * Stores a new paging API key hash.
   * @param apiKeyHash - Encoded scrypt hash.
   * @param apiKeyPrefix - Public key prefix shown in settings.
   * @param correlationId - Request correlation id.
   * @returns Updated key metadata.
   */
  async setApiKey(
    apiKeyHash: string,
    apiKeyPrefix: string,
    correlationId: string
  ): Promise<PagingApiKeyMetadata> {
    return this.request<PagingApiKeyMetadata>("setApiKey", correlationId, {
      apiKeyHash,
      apiKeyPrefix,
    });
  }

  /**
   * Clears paging API key metadata.
   * @param correlationId - Request correlation id.
   * @returns Updated key metadata.
   */
  async clearApiKey(correlationId: string): Promise<PagingApiKeyMetadata> {
    return this.request<PagingApiKeyMetadata>("clearApiKey", correlationId);
  }

  /**
   * Sends a request to the paging Unix socket API.
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
        if (settled) {
          return;
        }
        settled = true;
        socket.destroy();
        reject(new Error(`Paging socket timeout: ${method}`));
      }, this.timeoutMs);

      /**
       * Finishes the request by resolving or rejecting once.
       * @param error - Optional failure error.
       * @param data - Optional success payload.
       */
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
            const parsed = JSON.parse(line) as PagingSocketResponse<T> & { event?: string };
            if (parsed.event) {
              continue;
            }
            if (!parsed.ok) {
              finish(new Error(parsed.error?.message ?? `Paging request failed: ${method}`));
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
