import {
  createServer,
  type IncomingMessage,
  type Server,
  type ServerResponse,
} from "node:http";
import type { ServiceConfig } from "@homepi/core-config";
import type { Logger } from "@homepi/core-logging";
import {
  createSuccessResponse,
  createErrorResponse,
  getRequestCorrelationId,
} from "@homepi/core-api";
import { resolveCorrelationId } from "@homepi/core-logging";
import { EventBroadcaster } from "./event-broadcaster.js";
import {
  buildAudioRealtimeEnvelope,
  buildMetadataSnapshotEnvelope,
} from "./sse-subscribe-bootstrap.js";
import { SseHandler } from "./sse-handler.js";
import { WsHandler } from "./ws-handler.js";
import { buildCoreStatusPayload, buildHealthReportFromSnapshot } from "./core-status-builder.js";
import { HealthClient } from "./health/health-client.js";
import { buildRuntimeStatusPayload } from "./runtime-status-builder.js";
import type { SystemStatusStore } from "./system-status-store.js";
import type { UsbDevicesRoutes } from "./usb-devices/usb-devices-routes.js";
import type { HifiSerialRoutes } from "./hifi-serial/hifi-serial-routes.js";
import type { AudioRoutes } from "./audio/audio-routes.js";
import type { PagingRoutes } from "./audio/paging/paging-routes.js";
import type { PagingApiKeyRoutes } from "./audio/paging/paging-api-key-routes.js";
import { HifiSerialEventBridge } from "./hifi-serial/hifi-serial-event-bridge.js";
import { MetadataEventBridge } from "./metadata/metadata-event-bridge.js";
import { AudioRealtimeBridge } from "./audio/audio-realtime-bridge.js";
import type { MetadataClient } from "./metadata/metadata-client.js";
import { PcmRouterEventBridge } from "./pcm-router/pcm-router-event-bridge.js";
import { JournalLogBridge } from "./logging/journal-log-bridge.js";
import { StatusUpdateCoordinator } from "./status/status-update-coordinator.js";
import { readCpuTemperatureC } from "./system/read-cpu-temperature.js";
import { UsbDevicesEventBridge } from "./usb-devices/usb-devices-event-bridge.js";
import { EventsBrokerBridge } from "./events/events-broker-bridge.js";
import { isBrokerOnlyAudioSseEnabled } from "./audio/audio-ui-bridge.js";
import { AudioBrokerSnapshotStore } from "./audio/audio-broker-snapshot-store.js";

/**
 * HTTP server configuration for the backend vertical slice.
 */
export interface HttpServerOptions {
  /** Loaded service configuration. */
  config: ServiceConfig;
  /** Structured logger. */
  logger: Logger;
  /** System status store. */
  statusStore: SystemStatusStore;
  /** Process start timestamp. */
  startedAt: Date;
  /** Listen host. */
  host: string;
  /** Listen port. */
  port: number;
  /** Optional USB devices REST proxy routes. */
  usbRoutes?: UsbDevicesRoutes;
  /** Optional HiFi serial REST proxy routes. */
  hifiRoutes?: HifiSerialRoutes;
  /** Optional audio configuration REST routes. */
  audioRoutes?: AudioRoutes;
  /** Optional paging REST routes. */
  pagingRoutes?: PagingRoutes;
  /** Optional paging API key settings routes. */
  pagingApiKeyRoutes?: PagingApiKeyRoutes;
  /** Unix socket path for HiFi event bridge; omit to disable. */
  hifiSerialSocketPath?: string;
  /** Unix socket path for PCM router event bridge; omit to disable. */
  pcmRouterSocketPath?: string;
  /** Unix socket path for metadata event bridge; omit to disable. */
  metadataSocketPath?: string;
  /** Unix socket path for audio realtime progress bridge; omit to disable. */
  audioRealtimeSocketPath?: string;
  /** Unix socket path for USB devices event bridge; omit to disable. */
  usbDevicesSocketPath?: string;
  /** Unix socket path for homepi-health; omit to use default. */
  healthSocketPath?: string;
  /** Unix socket path for homepi-broker; preferred over legacy events broker. */
  brokerSocketPath?: string;
  /** @deprecated Legacy events.sock path — use brokerSocketPath. */
  eventsBrokerSocketPath?: string;
  /** Metadata socket client for SSE subscribe bootstrap. */
  metadataClient: MetadataClient;
  /** Shared broker snapshot cache for audio REST hydration. */
  brokerSnapshotStore?: AudioBrokerSnapshotStore;
}

/**
 * Creates and starts the HomePi backend HTTP server.
 * @param options - Server options.
 * @returns Node HTTP server instance.
 */
export function createHttpServer(options: HttpServerOptions): Server {
  const {
    config,
    logger,
    statusStore,
    startedAt,
    host,
    port,
    usbRoutes,
    hifiRoutes,
    audioRoutes,
    pagingRoutes,
    pagingApiKeyRoutes,
    hifiSerialSocketPath,
    pcmRouterSocketPath,
    metadataSocketPath,
    audioRealtimeSocketPath,
    usbDevicesSocketPath,
    eventsBrokerSocketPath,
    healthSocketPath,
    brokerSocketPath,
    metadataClient,
    brokerSnapshotStore,
  } = options;

  const brokerOnlyAudioSse = isBrokerOnlyAudioSseEnabled();
  const healthClient = new HealthClient(
    healthSocketPath ?? "/run/homepi/health/health.sock"
  );
  const brokerSocket =
    brokerSocketPath ??
    eventsBrokerSocketPath ??
    `${config.runtime.paths.socketDir}/broker/broker.sock`;

  const getStatus = () => statusStore.getStatus();
  let audioRealtimeBridge: AudioRealtimeBridge | undefined;
  const broadcaster = new EventBroadcaster(logger, getStatus, async (correlationId) => {
    const envelopes = [];
    const metadataSnapshot = await metadataClient
      .getSnapshot(correlationId)
      .catch(() => null);
    if (metadataSnapshot && metadataSnapshot.ownerZoneId > 0) {
      envelopes.push(buildMetadataSnapshotEnvelope(metadataSnapshot, correlationId));
    }
    const realtimeFrame = audioRealtimeBridge?.getLatestFrame();
    if (realtimeFrame && realtimeFrame.ownerZoneId > 0) {
      envelopes.push(buildAudioRealtimeEnvelope(realtimeFrame, correlationId));
    }
    return envelopes;
  });
  const sseHandler = new SseHandler(logger, broadcaster);
  const wsHandler = new WsHandler(logger, getStatus);
  const coordinator = new StatusUpdateCoordinator({
    statusStore,
    broadcaster,
    wsHandler,
  });

  const usbEventBridge = usbDevicesSocketPath
    ? new UsbDevicesEventBridge({
        socketPath: usbDevicesSocketPath,
        logger,
        broadcaster,
        coordinator,
      })
    : undefined;

  const hifiEventBridge =
    hifiSerialSocketPath
      ? new HifiSerialEventBridge({
        socketPath: hifiSerialSocketPath,
        logger,
        broadcaster,
        coordinator,
      })
    : undefined;

  const pcmEventBridge =
    pcmRouterSocketPath && !brokerOnlyAudioSse
      ? new PcmRouterEventBridge({
        socketPath: pcmRouterSocketPath,
        logger,
        broadcaster,
        coordinator,
      })
    : undefined;

  const metadataEventBridge =
    metadataSocketPath && !brokerOnlyAudioSse
      ? new MetadataEventBridge({
        socketPath: metadataSocketPath,
        logger,
        broadcaster,
        coordinator,
      })
    : undefined;

  const audioRealtimeBridgeInstance = audioRealtimeSocketPath
    ? new AudioRealtimeBridge({
        socketPath: audioRealtimeSocketPath,
        logger,
        broadcaster,
        coordinator,
        onConnectionChange: () => {},
      })
    : undefined;
  audioRealtimeBridge = audioRealtimeBridgeInstance;

  const eventsBrokerBridge = brokerSocket
    ? new EventsBrokerBridge({
        socketPath: brokerSocket,
        logger,
        broadcaster,
        coordinator,
        snapshotStore: brokerSnapshotStore,
      })
    : undefined;

  broadcaster.start();

  usbEventBridge?.start();
  hifiEventBridge?.start();
  pcmEventBridge?.start();
  metadataEventBridge?.start();
  audioRealtimeBridge?.start();
  eventsBrokerBridge?.start();

  const journalLogBridge = new JournalLogBridge({
    logger,
    broadcaster,
  });
  journalLogBridge.start();

  const server = createServer((req, res) => {
    handleRequest(req, res);
  });

  server.on("upgrade", (request, socket, head) => {
    const url = new URL(request.url ?? "/", `http://${request.headers.host ?? "localhost"}`);
    const correlationId = resolveCorrelationId(
      getRequestCorrelationId(request.headers)
    );

    if (isWebSocketPath(url.pathname)) {
      wsHandler.handleUpgrade(request, socket, head, correlationId);
      return;
    }

    socket.destroy();
  });

  server.listen(port, host, () => {
    logger.info({
      module: "app.backend",
      event: "service_started",
      correlationId: `startup-${Date.now()}`,
      message: "HomePi backend started",
      data: { host, port, environment: config.environment },
    });

    void pollCpuTemperature();
    setInterval(() => {
      void pollCpuTemperature();
    }, 5_000);
  });

  /**
   * Reads CPU temperature and broadcasts when the value changes.
   */
  async function pollCpuTemperature(): Promise<void> {
    const cpuTempC = await readCpuTemperatureC();
    coordinator.patchAndBroadcast({ cpuTempC }, "cpu-temperature");
  }

  server.on("close", () => {
    journalLogBridge.stop();
    usbEventBridge?.stop();
    hifiEventBridge?.stop();
    pcmEventBridge?.stop();
    broadcaster.stop();
    wsHandler.close();
  });

  return server;

  /**
   * Routes incoming HTTP requests.
   */
  function handleRequest(req: IncomingMessage, res: ServerResponse): void {
    const correlationId = resolveCorrelationId(
      getRequestCorrelationId(req.headers)
    );
    const url = new URL(req.url ?? "/", `http://${req.headers.host ?? "localhost"}`);

    if (usbRoutes?.matches(url.pathname)) {
      void usbRoutes.handle(req, res, url.pathname, correlationId);
      return;
    }

    if (hifiRoutes?.matches(url.pathname)) {
      void hifiRoutes.handle(req, res, url.pathname, correlationId);
      return;
    }

    if (pagingRoutes?.matches(url.pathname)) {
      void pagingRoutes.handle(req, res, url.pathname, correlationId);
      return;
    }

    if (pagingApiKeyRoutes?.matches(url.pathname)) {
      void pagingApiKeyRoutes.handle(req, res, url.pathname, correlationId);
      return;
    }

    if (audioRoutes?.matches(url.pathname)) {
      void audioRoutes.handle(req, res, url.pathname, correlationId);
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/health") {
      void handleHealth(res, correlationId);
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/runtime/status") {
      handleRuntimeStatus(res, correlationId);
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/core/status") {
      void handleCoreStatus(res, correlationId);
      return;
    }

    if (req.method === "GET" && isEventsPath(url.pathname)) {
      sseHandler.handle(req, res, correlationId);
      return;
    }

    if (req.method === "GET" && isWebSocketPath(url.pathname)) {
      sendJson(
        res,
        426,
        createErrorResponse({
          correlationId,
          error: {
            code: "UPGRADE_REQUIRED",
            message: "WebSocket upgrade required; connect with a WebSocket client",
          },
        })
      );
      return;
    }

    sendJson(
      res,
      404,
      createErrorResponse({
        correlationId,
        error: {
          code: "NOT_FOUND",
          message: `Route not found: ${url.pathname}`,
        },
      })
    );
  }

  /**
   * Returns system health proxied from homepi-health.
   */
  async function handleHealth(res: ServerResponse, correlationId: string): Promise<void> {
    const snapshot = await healthClient.getSnapshot(correlationId);
    const report = buildHealthReportFromSnapshot(config, snapshot);

    sendJson(
      res,
      snapshot.healthServiceReachable ? 200 : 503,
      createSuccessResponse({
        correlationId,
        data: {
          ...report,
          healthServiceReachable: snapshot.healthServiceReachable,
        } as unknown as Record<string, unknown>,
      })
    );
  }

  /**
   * Returns runtime status aligned with runtime-status.schema.json.
   */
  function handleRuntimeStatus(res: ServerResponse, correlationId: string): void {
    const runtimeStatus = buildRuntimeStatusPayload(config, startedAt);
    sendJson(
      res,
      200,
      createSuccessResponse({
        correlationId,
        data: runtimeStatus as unknown as Record<string, unknown>,
      })
    );
  }

  /**
   * Returns hierarchical module health proxied from homepi-health.
   */
  async function handleCoreStatus(res: ServerResponse, correlationId: string): Promise<void> {
    const snapshot = await healthClient.getSnapshot(correlationId);
    const host = statusStore.getStatus();
    const payload = buildCoreStatusPayload(config, snapshot, {
      uptimeMs: host.uptimeMs,
      cpuTempC: host.cpuTempC,
      lastEventAt: host.lastEventAt,
    });

    sendJson(
      res,
      snapshot.healthServiceReachable ? 200 : 503,
      createSuccessResponse({
        correlationId,
        data: payload as unknown as Record<string, unknown>,
      })
    );
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
 * Returns whether a path is an SSE events endpoint.
 * @param pathname - Request pathname.
 * @returns True when path serves SSE.
 */
function isEventsPath(pathname: string): boolean {
  return pathname === "/events" || pathname === "/api/events";
}

/**
 * Returns whether a path is a WebSocket endpoint.
 * @param pathname - Request pathname.
 * @returns True when path serves WebSocket upgrades.
 */
function isWebSocketPath(pathname: string): boolean {
  return pathname === "/ws" || pathname === "/api/ws";
}
