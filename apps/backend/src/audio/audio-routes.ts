import { readFile } from "node:fs/promises";
import type { IncomingMessage, ServerResponse } from "node:http";

import type { ServiceConfig } from "@homepi/core-config";
import type { Logger } from "@homepi/core-logging";
import { createErrorResponse, createSuccessResponse } from "@homepi/core-api";

import type { HifiSerialClient } from "../hifi-serial/hifi-serial-client.js";
import type { MetadataClient } from "../metadata/metadata-client.js";
import type { PcmRouterClient } from "../pcm-router/pcm-router-client.js";
import type { SystemStatusStore } from "../system-status-store.js";
import { buildAudioSnapshot } from "./build-audio-snapshot.js";
import {
  requiresShairportRestart,
  type ShairportZonePatch,
  type ZoneControllerPatch,
} from "./hifi-zone-commands.js";
import {
  type SourceControllerPatch,
} from "./hifi-source-commands.js";
import type { ShairportRemoteClient } from "./shairport-remote-client.js";
import type { AudioBrokerSnapshotStore } from "./audio-broker-snapshot-store.js";
import { percentToAppleDb } from "./volume-conversion.js";

/**
 * Dependencies for audio configuration HTTP handlers.
 */
export interface AudioRouteDeps {
  /** HiFi serial socket client. */
  client: HifiSerialClient;
  /** PCM router socket client. */
  pcmClient: PcmRouterClient;
  /** Metadata socket client. */
  metadataClient: MetadataClient;
  /** Shairport MQTT remote-control client. */
  shairportRemote: ShairportRemoteClient;
  /** System status store. */
  statusStore: SystemStatusStore;
  /** Loaded service configuration. */
  config: ServiceConfig;
  /** Structured logger. */
  logger: Logger;
  /** Optional broker snapshot cache for REST hydration. */
  brokerSnapshotStore?: AudioBrokerSnapshotStore;
}

/**
 * Registers audio configuration REST handlers.
 */
export class AudioRoutes {
  /**
   * Creates route handlers.
   * @param deps - Handler dependencies.
   */
  constructor(private readonly deps: AudioRouteDeps) {}

  /**
   * Returns true when the pathname is an audio API route.
   * @param pathname - Request path.
   * @returns Match result.
   */
  matches(pathname: string): boolean {
    return pathname.startsWith("/api/audio");
  }

  /**
   * Handles an audio HTTP request.
   * @param req - Incoming request.
   * @param res - HTTP response.
   * @param pathname - Request pathname.
   * @param correlationId - Correlation id.
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
      if (req.method === "GET" && pathname === "/api/audio/snapshot") {
        const snapshot = await buildAudioSnapshot(
          {
            config: this.deps.config,
            hifiClient: this.deps.client,
            pcmClient: this.deps.pcmClient,
            metadataClient: this.deps.metadataClient,
            shairportRemote: this.deps.shairportRemote,
            systemStatus: this.deps.statusStore.getStatus(),
            brokerSnapshotStore: this.deps.brokerSnapshotStore,
          },
          correlationId
        );
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: snapshot as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      if (req.method === "GET" && pathname === "/api/audio/history") {
        const url = new URL(req.url ?? "", "http://localhost");
        const limitParam = Number(url.searchParams.get("limit") ?? "20");
        const limit = Number.isFinite(limitParam) && limitParam > 0 ? Math.min(limitParam, 20) : 20;
        const history = await this.deps.metadataClient.getHistory(correlationId, limit);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: (history ?? { limit: 0, entries: [] }) as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      const historyCoverMatch = pathname.match(/^\/api\/audio\/history\/(\d+)\/cover$/);
      if (req.method === "GET" && historyCoverMatch) {
        const historyId = Number(historyCoverMatch[1]);
        const history = await this.deps.metadataClient.getHistory(correlationId, 20);
        const entry = history?.entries.find((row) => row.id === historyId);
        const cacheDir = this.deps.config.runtime.paths.cacheDir;
        const coverCandidates = [
          entry?.coverArtId
            ? `${cacheDir}/metadata/artwork/sha256-${entry.coverArtId}.jpg`
            : "",
          `${cacheDir}/metadata/artwork/current.jpg`,
        ].filter(Boolean);
        const cover = await readFirstCover(coverCandidates);
        if (!cover || cover.length === 0) {
          res.writeHead(404, { "Content-Length": "0" });
          res.end();
          return true;
        }
        sendCoverResponse(res, cover);
        return true;
      }

      if (req.method === "GET" && pathname === "/api/audio/now-playing") {
        const [snapshot, pcmSnapshot] = await Promise.all([
          this.deps.metadataClient.getSnapshot(correlationId),
          this.deps.pcmClient.getSnapshot(correlationId).catch(() => null),
        ]);
        const ownerZoneId = pcmSnapshot?.ownerZoneId ?? 0;
        const activeStack = pcmSnapshot?.activeStack ?? [];
        const hasActiveRoute = ownerZoneId > 0 || activeStack.length > 0;
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: (hasActiveRoute
              ? snapshot
              : {
                  ownerZoneId: 0,
                  zoneId: 0,
                  playing: false,
                  positionMs: 0,
                  durationMs: 0,
                  hasCoverArt: false,
                }) as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      if (req.method === "GET" && pathname === "/api/audio/now-playing/cover") {
        const url = new URL(req.url ?? "", "http://localhost");
        const version = url.searchParams.get("v") ?? "";
        const cacheDir = this.deps.config.runtime.paths.cacheDir;
        const coverCandidates = [
          version.startsWith("sha256-")
            ? `${cacheDir}/metadata/artwork/${version}.jpg`
            : "",
          `${cacheDir}/metadata/artwork/current.jpg`,
          `${cacheDir}/current.jpg`,
        ].filter(Boolean);
        const cover = await readFirstCover(coverCandidates);
        if (!cover || cover.length === 0) {
          res.writeHead(404, { "Content-Length": "0" });
          res.end();
          return true;
        }
        sendCoverResponse(res, cover);
        return true;
      }

      if (req.method === "GET" && pathname === "/api/audio/airplay-source") {
        const result = await this.deps.client.getAirplaySource(correlationId);
        sendJson(res, 200, createSuccessResponse({ correlationId, data: result }));
        return true;
      }

      if (req.method === "POST" && pathname === "/api/audio/controller/sync") {
        const result = await this.deps.client.syncController(correlationId);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: result as unknown as Record<string, unknown>,
          })
        );
        return true;
      }

      if (req.method === "PUT" && pathname === "/api/audio/controller") {
        const body = JSON.parse(await readBody(req)) as { deviceName?: string };
        const deviceName = typeof body.deviceName === "string" ? body.deviceName.trim() : "";
        if (!deviceName) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "deviceName is required" },
            })
          );
          return true;
        }

        await this.deps.client.patchController({ deviceName }, correlationId);
        await this.deps.client.executeHifiCommand(
          "set_controller_netname",
          { deviceName },
          correlationId
        );

        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: { deviceName },
          })
        );
        return true;
      }

      if (req.method === "POST" && pathname === "/api/audio/playback/remote") {
        const body = JSON.parse(await readBody(req)) as {
          zoneId?: number;
          command?: string;
        };
        if (typeof body.command !== "string" || !this.deps.shairportRemote.isAllowedCommand(body.command)) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "command is required and must be valid" },
            })
          );
          return true;
        }

        let zoneId = body.zoneId;
        if (zoneId === undefined) {
          const pcmSnapshot = await this.deps.pcmClient.getSnapshot(correlationId);
          zoneId = pcmSnapshot?.ownerZoneId ?? 0;
        }
        if (typeof zoneId !== "number" || zoneId < 1 || zoneId > 16) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "zoneId must be 1-16" },
            })
          );
          return true;
        }

        await this.deps.shairportRemote.publishRemoteCommand(zoneId, body.command);
        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: { zoneId, command: body.command },
          })
        );
        return true;
      }

      if (req.method === "POST" && pathname === "/api/audio/playback/volume") {
        const body = JSON.parse(await readBody(req)) as {
          zoneId?: number;
          volume?: number;
        };
        if (typeof body.volume !== "number") {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "volume is required" },
            })
          );
          return true;
        }

        let zoneId = body.zoneId;
        if (zoneId === undefined) {
          const pcmSnapshot = await this.deps.pcmClient.getSnapshot(correlationId);
          zoneId = pcmSnapshot?.ownerZoneId ?? 0;
        }
        if (typeof zoneId !== "number" || zoneId < 1 || zoneId > 16) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "zoneId must be 1-16" },
            })
          );
          return true;
        }

        const clamped = Math.max(0, Math.min(100, Math.round(body.volume)));
        await this.deps.client.patchZoneController(
          zoneId,
          { volume: clamped },
          correlationId
        );
        await this.deps.client.executeHifiCommand(
          "apply_zone_controller_patch",
          { zoneNumber: zoneId, volume: clamped },
          correlationId
        );

        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: {
              zoneId,
              volume: clamped,
              airplayDb: percentToAppleDb(clamped),
            },
          })
        );
        return true;
      }

      const coverMatch = pathname.match(/^\/api\/audio\/playback\/cover\/(\d+)$/);
      if (req.method === "GET" && coverMatch) {
        const zoneId = Number(coverMatch[1]);
        if (zoneId < 1 || zoneId > 16) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "zone must be 1-16" },
            })
          );
          return true;
        }

        const cacheDir = this.deps.config.runtime.paths.cacheDir;
        const coverCandidates = [
          `${cacheDir}/metadata/artwork/current.jpg`,
          `${cacheDir}/current.jpg`,
          `${cacheDir}/cover-zone-${zoneId}`,
        ];
        let cover = await readFirstCover(coverCandidates);
        if (!cover) {
          cover = await this.deps.shairportRemote.fetchCoverArt(zoneId);
        }
        if (!cover || cover.length === 0) {
          res.writeHead(404, { "Content-Length": "0" });
          res.end();
          return true;
        }

        sendCoverResponse(res, cover);
        return true;
      }

      if (req.method === "PUT" && pathname === "/api/audio/airplay-source") {
        const body = await readBody(req);
        const parsed = JSON.parse(body) as { sourceNumber?: number };
        if (typeof parsed.sourceNumber !== "number") {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "sourceNumber is required" },
            })
          );
          return true;
        }
        const result = await this.deps.client.setAirplaySource(
          parsed.sourceNumber,
          correlationId
        );
        sendJson(res, 200, createSuccessResponse({ correlationId, data: result }));
        return true;
      }

      const zoneMatch = pathname.match(/^\/api\/audio\/zones\/(\d+)$/);
      if (req.method === "PUT" && zoneMatch) {
        const zoneNumber = Number(zoneMatch[1]);
        if (zoneNumber < 1 || zoneNumber > 16) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "zone must be 1-16" },
            })
          );
          return true;
        }

        const body = JSON.parse(await readBody(req)) as {
          controller?: ZoneControllerPatch;
          shairport?: ShairportZonePatch;
        };

        const controllerPatch = body.controller ?? {};
        const shairportPatch = body.shairport ?? {};

        if (Object.keys(controllerPatch).length > 0) {
          await this.deps.client.patchZoneController(
            zoneNumber,
            controllerPatch as Record<string, unknown>,
            correlationId
          );
          await this.deps.client.executeHifiCommand(
            "apply_zone_controller_patch",
            { zoneNumber, ...controllerPatch },
            correlationId
          );
        }

        if (controllerPatch.enabled !== undefined) {
          await this.deps.pcmClient.setZoneEnabled(
            zoneNumber,
            controllerPatch.enabled === 1,
            correlationId
          );
        }

        if (Object.keys(shairportPatch).length > 0) {
          await this.deps.client.updateShairportZoneSettings(
            zoneNumber,
            shairportPatch,
            correlationId
          );
        }

        const shairportRestartRequired = requiresShairportRestart(
          controllerPatch,
          shairportPatch
        );

        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: { zoneNumber, shairportRestartRequired },
          })
        );
        return true;
      }

      const sourceMatch = pathname.match(/^\/api\/audio\/sources\/(\d+)$/);
      if (req.method === "PUT" && sourceMatch) {
        const sourceNumber = Number(sourceMatch[1]);
        if (sourceNumber < 1 || sourceNumber > 8) {
          sendJson(
            res,
            400,
            createErrorResponse({
              correlationId,
              error: { code: "INVALID_REQUEST", message: "source must be 1-8" },
            })
          );
          return true;
        }

        const body = JSON.parse(await readBody(req)) as {
          controller?: SourceControllerPatch;
          airplay?: boolean;
        };

        const controllerPatch = body.controller ?? {};
        const wantsAirplay = body.airplay === true;

        if (wantsAirplay) {
          const sourcesResult = await this.deps.client.getSources(correlationId);
          const sources = sourcesResult.sources as Array<{
            sourceNumber?: number;
            enabled?: number;
          }>;
          const sourceRow = sources.find((row) => row.sourceNumber === sourceNumber);
          const enabled =
            controllerPatch.enabled !== undefined
              ? controllerPatch.enabled
              : sourceRow?.enabled;
          if (enabled !== 1) {
            sendJson(
              res,
              400,
              createErrorResponse({
                correlationId,
                error: {
                  code: "INVALID_REQUEST",
                  message: "AirPlay source must be enabled",
                },
              })
            );
            return true;
          }
        }

        if (Object.keys(controllerPatch).length > 0) {
          await this.deps.client.patchSource(
            sourceNumber,
            controllerPatch as Record<string, unknown>,
            correlationId
          );
          await this.deps.client.executeHifiCommand(
            "apply_source_patch",
            { sourceNumber, ...controllerPatch },
            correlationId
          );
        }

        let shairportRestartRequired = false;
        if (wantsAirplay) {
          const currentAirplay = await this.deps.client.getAirplaySource(correlationId);
          if (currentAirplay.sourceNumber !== sourceNumber) {
            shairportRestartRequired = true;
          }
          await this.deps.client.setAirplaySource(sourceNumber, correlationId);
        }

        sendJson(
          res,
          200,
          createSuccessResponse({
            correlationId,
            data: { sourceNumber, shairportRestartRequired },
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
      const raw = error instanceof Error ? error.message : "Audio request failed";
      const message = raw.includes("ECONNREFUSED")
        ? "A native audio service is restarting after configuration save. Please try again."
        : raw;
      this.deps.logger.warn({
        module: "app.backend.audio",
        event: "request_failed",
        correlationId,
        message: raw,
      });
      sendJson(
        res,
        503,
        createErrorResponse({
          correlationId,
          error: { code: "AUDIO_UNAVAILABLE", message },
        })
      );
      return true;
    }
  }
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
 * Reads the first non-empty cover image from candidate paths.
 * @param paths - Candidate filesystem paths.
 * @returns Cover bytes or null.
 */
async function readFirstCover(paths: string[]): Promise<Buffer | null> {
  for (const cachePath of paths) {
    try {
      const cached = await readFile(cachePath);
      if (cached.length > 0) {
        return cached;
      }
    } catch {
      continue;
    }
  }
  return null;
}

/**
 * Writes a cover image HTTP response.
 * @param res - HTTP response.
 * @param cover - Cover bytes.
 */
function sendCoverResponse(res: ServerResponse, cover: Buffer): void {
  const contentType = detectCoverContentType(cover);
  res.writeHead(200, {
    "Content-Type": contentType,
    "Content-Length": cover.length,
    "Cache-Control": "no-cache",
  });
  res.end(cover);
}

/**
 * Detects JPEG or PNG cover art for the HTTP Content-Type header.
 * @param cover - Raw cover image bytes.
 * @returns MIME type for the response.
 */
function detectCoverContentType(cover: Buffer): string {
  if (cover.length >= 3 && cover[0] === 0xff && cover[1] === 0xd8 && cover[2] === 0xff) {
    return "image/jpeg";
  }
  if (
    cover.length >= 8 &&
    cover[0] === 0x89 &&
    cover[1] === 0x50 &&
    cover[2] === 0x4e &&
    cover[3] === 0x47
  ) {
    return "image/png";
  }
  return "application/octet-stream";
}
