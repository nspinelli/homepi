import type { IncomingMessage, ServerResponse } from "node:http";
import { connect } from "node:net";
import { createRequest } from "@homepi/core-messaging";
import { encodeNdjsonLine } from "@homepi/core-transport";

import { createErrorResponse, createSuccessResponse } from "@homepi/core-api";
import { createEventEnvelope } from "@homepi/core-events";
import type { Logger } from "@homepi/core-logging";

import { verifyPagingApiKey } from "./paging-api-key.js";
import type { PagingClient } from "./paging-client.js";
import type {
  PagingApiKeyMetadata,
  PagingChimeRequest,
  PagingConfigUpdate,
  PagingPagePreviewRequest,
  PagingSpeakRequest,
  PagingVoicePreviewRequest,
} from "./paging-types.js";
import { installCatalogVoice } from "./voice-installer.js";
import { isEnglishVoice, readPagingVoiceCatalog, resolvePagingVoiceSampleUrl } from "./voice-catalog.js";
import { saveChimeUpload } from "./paging-chime-upload.js";

/**
 * Dependencies for paging REST handlers.
 */
export interface PagingRouteDeps {
  /** Paging service Unix socket client. */
  client: PagingClient;
  /** Structured logger. */
  logger: Logger;
  /** Unix socket path for core/events publish calls. */
  eventsSocketPath: string;
}

/**
 * Registers paging REST handlers under /api/audio/paging.
 */
export class PagingRoutes {
  /**
   * Creates route handlers.
   * @param deps - Handler dependencies.
   */
  constructor(private readonly deps: PagingRouteDeps) {}

  /**
   * Returns true when the pathname is a paging API route.
   * @param pathname - Request path.
   * @returns True when this class should handle the request.
   */
  matches(pathname: string): boolean {
    return pathname.startsWith("/api/audio/paging");
  }

  /**
   * Handles a paging HTTP request.
   * @param req - Incoming request.
   * @param res - HTTP response.
   * @param pathname - Request pathname.
   * @param correlationId - Request correlation id.
   * @returns True when handled.
   */
  async handle(
    req: IncomingMessage,
    res: ServerResponse,
    pathname: string,
    correlationId: string
  ): Promise<boolean> {
    if (!this.matches(pathname)) {
      return false;
    }

    try {
      if (req.method === "GET" && pathname === "/api/audio/paging/config") {
        const config = await this.deps.client.getConfig(correlationId);
        sendJson(res, 200, createSuccessResponse({ correlationId, data: config as unknown as Record<string, unknown> }));
        return true;
      }

      if (req.method === "PUT" && pathname === "/api/audio/paging/config") {
        const body = JSON.parse(await readBody(req)) as PagingConfigUpdate;
        const updated = await this.deps.client.updateConfig(body, correlationId);
        sendJson(res, 200, createSuccessResponse({ correlationId, data: updated as unknown as Record<string, unknown> }));
        return true;
      }

      if (req.method === "GET" && pathname === "/api/audio/paging/status") {
        const status = await this.deps.client.getStatus(correlationId);
        sendJson(res, 200, createSuccessResponse({ correlationId, data: status as unknown as Record<string, unknown> }));
        return true;
      }

      if (req.method === "GET" && pathname === "/api/audio/paging/voices/catalog") {
        const [installedVoicesResult, config, catalog] = await Promise.all([
          this.deps.client.getVoices(correlationId),
          this.deps.client.getConfig(correlationId),
          readPagingVoiceCatalog(),
        ]);
        const installedVoices = new Map(
          installedVoicesResult.voices.map((voice) => [voice.voiceId, voice])
        );
        const voices = catalog.voices
          .filter(isEnglishVoice)
          .map((voice) => {
            const installed = installedVoices.get(voice.voiceId);
            return {
              voiceId: voice.voiceId,
              displayName: voice.displayName,
              languageCode: voice.languageCode,
              accent: voice.accent,
              quality: voice.quality,
              isBundled: voice.isBundled,
              sampleAvailable: voice.sampleAvailable,
              sampleUrl: resolvePagingVoiceSampleUrl(voice),
              installed: installed?.installed === true,
              isDefault: config.defaultVoiceId === voice.voiceId,
            };
          });
        sendJson(res, 200, createSuccessResponse({ correlationId, data: { voices } }));
        return true;
      }

      if (req.method === "POST" && pathname === "/api/audio/paging/voices/install") {
        const body = JSON.parse(await readBody(req)) as {
          voiceId?: string;
          setDefault?: boolean;
        };
        if (!body.voiceId) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "voiceId is required" },
            })
          );
          return true;
        }
        try {
          const data = await installCatalogVoice({
            client: this.deps.client,
            voiceId: body.voiceId,
            setDefault: body.setDefault,
            correlationId,
            logger: this.deps.logger,
          });
          sendJson(res, 200, createSuccessResponse({ correlationId, data }));
        } catch (error) {
          const message = error instanceof Error ? error.message : "Failed to install voice";
          const code =
            error instanceof Error && "code" in error && typeof error.code === "string"
              ? error.code
              : "PAGING_VOICE_INSTALL_FAILED";
          sendJson(
            res,
            code === "max_installed_voices_reached" ? 409 : 500,
            createErrorResponse({
              correlationId,
              error: { code, message },
            })
          );
        }
        return true;
      }

      const voiceDeleteMatch = pathname.match(/^\/api\/audio\/paging\/voices\/([^/]+)$/);
      if (req.method === "DELETE" && voiceDeleteMatch) {
        const voiceId = decodeURIComponent(voiceDeleteMatch[1]);
        const data = await this.deps.client.removeVoice(voiceId, correlationId);
        sendJson(res, 200, createSuccessResponse({ correlationId, data }));
        return true;
      }

      if (req.method === "GET" && pathname === "/api/audio/paging/chimes") {
        const data = await this.deps.client.getChimes(correlationId);
        sendJson(res, 200, createSuccessResponse({ correlationId, data }));
        return true;
      }

      if (req.method === "POST" && pathname === "/api/audio/paging/chimes/upload") {
        let body: { displayName?: string; wavBase64?: string };
        try {
          body = JSON.parse(await readBody(req)) as { displayName?: string; wavBase64?: string };
        } catch {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: {
                code: "INVALID_REQUEST",
                message: "Request body must be JSON with displayName and wavBase64",
              },
            })
          );
          return true;
        }

        if (!body.displayName || !body.wavBase64) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: {
                code: "INVALID_REQUEST",
                message: "displayName and wavBase64 are required",
              },
            })
          );
          return true;
        }

        try {
          const wavBuffer = Buffer.from(body.wavBase64, "base64");
          const saved = await saveChimeUpload(
            body.displayName,
            wavBuffer,
            "/var/lib/homepi/paging/chimes"
          );
          const data = await this.deps.client.uploadChime(
            {
              chimeId: saved.chimeId,
              displayName: saved.displayName,
              filePath: saved.filePath,
              durationMs: saved.durationMs,
            },
            correlationId
          );
          sendJson(
            res,
            200,
            createSuccessResponse({
              correlationId,
              data: {
                ...data,
                chimeId: saved.chimeId,
                displayName: saved.displayName,
                durationMs: saved.durationMs,
                sizeBytes: saved.sizeBytes,
              },
            })
          );
        } catch (error) {
          const message = error instanceof Error ? error.message : "Failed to upload chime";
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "PAGING_CHIME_UPLOAD_FAILED", message },
            })
          );
        }
        return true;
      }

      if (req.method === "POST" && pathname === "/api/audio/paging/chimes/preview") {
        const body = JSON.parse(await readBody(req)) as { chimeId?: string };
        if (!body.chimeId) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "chimeId is required" },
            })
          );
          return true;
        }

        const commandPayload = {
          chimeId: body.chimeId,
          source: "ui",
          onBusy: "reject",
          waitUntil: "accepted",
        };
        await this.publishPagingCommand("audio.paging.command.chime", correlationId, commandPayload);
        sendJson(
          res,
          202,
          createSuccessResponse({
            correlationId,
            data: { ok: true, status: "accepted", command: "chime" },
          })
        );
        return true;
      }

      const activeChimeMatch = pathname.match(/^\/api\/audio\/paging\/chimes\/([^/]+)\/active$/);
      if (req.method === "PUT" && activeChimeMatch) {
        const chimeId = decodeURIComponent(activeChimeMatch[1]);
        const data = await this.deps.client.setActiveChime(chimeId, correlationId);
        sendJson(res, 200, createSuccessResponse({ correlationId, data }));
        return true;
      }

      const deleteChimeMatch = pathname.match(/^\/api\/audio\/paging\/chimes\/([^/]+)$/);
      if (req.method === "DELETE" && deleteChimeMatch) {
        const chimeId = decodeURIComponent(deleteChimeMatch[1]);
        const data = await this.deps.client.removeChime(chimeId, correlationId);
        sendJson(res, 200, createSuccessResponse({ correlationId, data }));
        return true;
      }

      if (req.method === "POST" && pathname === "/api/audio/paging/voices/preview") {
        const body = JSON.parse(await readBody(req)) as PagingVoicePreviewRequest;
        if (!body.voiceId || !body.text) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "voiceId and text are required" },
            })
          );
          return true;
        }
        await this.publishPagingCommand("audio.paging.command.preview_voice", correlationId, {
          ...body,
          output: body.output ?? "paging_dac_only",
          source: "api",
        });
        sendJson(
          res,
          202,
          createSuccessResponse({
            correlationId,
            data: { ok: true, status: "accepted", command: "preview_voice" },
          })
        );
        return true;
      }

      if (req.method === "POST" && pathname === "/api/audio/paging/preview-page") {
        const body = JSON.parse(await readBody(req)) as PagingPagePreviewRequest;
        if (!body.text) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "text is required" },
            })
          );
          return true;
        }
        await this.publishPagingCommand("audio.paging.command.preview_page", correlationId, {
          ...body,
          source: "api",
          includeChime: body.includeChime ?? false,
        });
        sendJson(
          res,
          202,
          createSuccessResponse({
            correlationId,
            data: { ok: true, status: "accepted", command: "preview_page" },
          })
        );
        return true;
      }

      if (req.method === "POST" && pathname === "/api/audio/paging/speak") {
        const body = JSON.parse(await readBody(req)) as PagingSpeakRequest;
        if (!body.text) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "text is required" },
            })
          );
          return true;
        }
        const isAuthorized = await this.authorizePagingCommand(req, correlationId);
        if (!isAuthorized) {
          sendJson(
            res,
            401,
            createErrorResponse({
              correlationId,
              error: { code: "UNAUTHORIZED", message: "unauthorized" },
            })
          );
          return true;
        }
        await this.publishPagingCommand("audio.paging.command.speak", correlationId, {
          ...body,
          source: body.source ?? "api",
          includeChime: body.includeChime ?? false,
          onBusy: body.onBusy ?? "reject",
          waitUntil: body.waitUntil ?? "accepted",
        });
        sendJson(
          res,
          202,
          createSuccessResponse({
            correlationId,
            data: { ok: true, status: "started", command: "speak" },
          })
        );
        return true;
      }

      if (req.method === "POST" && pathname === "/api/audio/paging/chime") {
        const body = JSON.parse(await readBody(req)) as PagingChimeRequest;
        const isAuthorized = await this.authorizePagingCommand(req, correlationId);
        if (!isAuthorized) {
          sendJson(
            res,
            401,
            createErrorResponse({
              correlationId,
              error: { code: "UNAUTHORIZED", message: "unauthorized" },
            })
          );
          return true;
        }
        await this.publishPagingCommand("audio.paging.command.chime", correlationId, {
          chimeId: body.chimeId ?? null,
          source: body.source ?? "api",
          onBusy: body.onBusy ?? "reject",
          waitUntil: body.waitUntil ?? "accepted",
        });
        sendJson(
          res,
          202,
          createSuccessResponse({
            correlationId,
            data: { ok: true, status: "started", command: "chime" },
          })
        );
        return true;
      }

      sendJson(
        res,
        404,
        createErrorResponse({
          correlationId,
          error: { code: "NOT_FOUND", message: `Route not found: ${pathname}` },
        })
      );
      return true;
    } catch (error) {
      const message = error instanceof Error ? error.message : "Paging request failed";
      this.deps.logger.warn({
        module: "app.backend.paging",
        event: "request_failed",
        correlationId,
        message,
      });
      sendJson(
        res,
        503,
        createErrorResponse({
          correlationId,
          error: { code: "PAGING_UNAVAILABLE", message },
        })
      );
      return true;
    }
  }

  /**
   * Authenticates API callers for speak/chime automation routes.
   * @param req - Incoming request.
   * @param correlationId - Request correlation id.
   * @returns True when the supplied key is valid.
   */
  private async authorizePagingCommand(
    req: IncomingMessage,
    correlationId: string
  ): Promise<boolean> {
    const presentedKey = extractPagingApiKey(req);
    if (!presentedKey) {
      return false;
    }
    const metadata = await this.deps.client.getApiKey(correlationId);
    return verifyPagingApiKey(presentedKey, resolveApiKeyHash(metadata));
  }

  /**
   * Publishes a paging command envelope to the core/events broker.
   * @param type - Paging command type name.
   * @param correlationId - Request correlation id.
   * @param payload - Command payload object.
   */
  private async publishPagingCommand(
    type: string,
    correlationId: string,
    payload: Record<string, unknown>
  ): Promise<void> {
    const event = type.replace(/^audio\.paging\.command\./, "");
    const envelope = createEventEnvelope({
      source: "homepi-backend",
      topic: "audio.paging.command",
      event,
      correlationId,
      payload: {
        ...payload,
        source: "api",
      },
    });

    await publishEventEnvelope(this.deps.eventsSocketPath, envelope);
  }
}

/**
 * Sends an event envelope as a single frame to core/events.
 * @param socketPath - Broker Unix socket path.
 * @param envelope - Event envelope to publish.
 */
async function publishEventEnvelope(socketPath: string, envelope: object): Promise<void> {
  const legacyEnvelope = envelope as {
    topic: string;
    source: string;
    event: string;
    correlationId: string;
    payload?: Record<string, unknown>;
  };
  const request = createRequest({
    source: "homepi-backend",
    target: "homepi-broker",
    command: "publish",
    correlationId: legacyEnvelope.correlationId,
    payload: {
      topic: legacyEnvelope.topic,
      source: legacyEnvelope.source,
      severity: "info",
      eventPayload: {
        ...(legacyEnvelope.payload ?? {}),
        event: legacyEnvelope.event,
      },
    },
  });

  await new Promise<void>((resolve, reject) => {
    const socket = connect(socketPath);
    let settled = false;

    /**
     * Completes publish flow exactly once.
     * @param error - Optional publish error.
     */
    const finish = (error?: Error): void => {
      if (settled) {
        return;
      }
      settled = true;
      socket.destroy();
      if (error) {
        reject(error);
        return;
      }
      resolve();
    };

    socket.on("error", (error) => finish(error));
    socket.on("connect", () => {
      socket.write(encodeNdjsonLine(request), (error) => {
        if (error) {
          finish(error);
          return;
        }
        finish();
      });
    });
  });
}

/**
 * Extracts API key hash from paging metadata.
 * @param metadata - Paging API key metadata payload.
 * @returns Stored encoded hash or null.
 */
function resolveApiKeyHash(metadata: PagingApiKeyMetadata): string | null {
  if (typeof metadata.hash === "string" && metadata.hash.length > 0) {
    return metadata.hash;
  }
  return null;
}

/**
 * Extracts a paging API key from accepted HTTP headers.
 * @param req - Incoming request.
 * @returns Raw API key value or null.
 */
function extractPagingApiKey(req: IncomingMessage): string | null {
  const headerValue = req.headers["x-homepi-paging-key"];
  if (typeof headerValue === "string" && headerValue.trim()) {
    return headerValue.trim();
  }

  const authHeader = req.headers.authorization;
  if (!authHeader) {
    return null;
  }
  const raw = Array.isArray(authHeader) ? authHeader[0] : authHeader;
  const match = raw.match(/^Bearer\s+(.+)$/i);
  if (!match) {
    return null;
  }
  return match[1].trim() || null;
}

/**
 * Reads the request body as UTF-8 text.
 * @param req - Incoming request.
 * @returns Body string.
 */
function readBody(req: IncomingMessage): Promise<string> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    req.on("data", (chunk) => chunks.push(Buffer.from(chunk)));
    req.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
    req.on("error", reject);
  });
}

/**
 * Sends a JSON API response envelope.
 * @param res - HTTP response.
 * @param status - HTTP status code.
 * @param body - Response body.
 */
function sendJson(res: ServerResponse, status: number, body: unknown): void {
  const payload = JSON.stringify(body);
  res.writeHead(status, {
    "Content-Type": "application/json",
    "Content-Length": Buffer.byteLength(payload),
  });
  res.end(payload);
}
