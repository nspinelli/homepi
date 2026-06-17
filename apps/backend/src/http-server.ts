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
import { createHealthReport } from "@homepi/core-health";
import { resolveCorrelationId } from "@homepi/core-logging";
import { EventBroadcaster } from "./event-broadcaster.js";
import { SseHandler } from "./sse-handler.js";
import { WsHandler } from "./ws-handler.js";
import { buildCoreStatusPayload } from "./core-status-builder.js";
import { buildRuntimeStatusPayload } from "./runtime-status-builder.js";
import type { SystemStatusStore } from "./system-status-store.js";
import type { UsbDevicesRoutes } from "./usb-devices/usb-devices-routes.js";
import type { HifiSerialRoutes } from "./hifi-serial/hifi-serial-routes.js";
import type { AudioRoutes } from "./audio/audio-routes.js";
import { HifiSerialEventBridge } from "./hifi-serial/hifi-serial-event-bridge.js";
import type { HifiSerialClient } from "./hifi-serial/hifi-serial-client.js";
import { MetadataEventBridge } from "./metadata/metadata-event-bridge.js";
import type { MetadataClient } from "./metadata/metadata-client.js";
import { PcmRouterEventBridge } from "./pcm-router/pcm-router-event-bridge.js";
import type { PcmRouterClient } from "./pcm-router/pcm-router-client.js";
import { JournalLogBridge } from "./logging/journal-log-bridge.js";
import { FallbackReconciliation } from "./status/fallback-reconciliation.js";
import { resolveFallbackReconciliation } from "./status/resolve-fallback-reconciliation.js";
import { JournalServiceStatusBridge } from "./status/journal-service-status-bridge.js";
import {
  createStartupSnapshotLoaders,
  loadStartupSnapshots,
} from "./status/startup-snapshots.js";
import { StatusUpdateCoordinator } from "./status/status-update-coordinator.js";
import { readCpuTemperatureC } from "./system/read-cpu-temperature.js";
import { UsbDevicesEventBridge } from "./usb-devices/usb-devices-event-bridge.js";
import type { UsbDevicesClient } from "./usb-devices/usb-devices-client.js";

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
  /** Unix socket path for HiFi event bridge; omit to disable. */
  hifiSerialSocketPath?: string;
  /** Unix socket path for PCM router event bridge; omit to disable. */
  pcmRouterSocketPath?: string;
  /** Unix socket path for metadata event bridge; omit to disable. */
  metadataSocketPath?: string;
  /** Unix socket path for USB devices event bridge; omit to disable. */
  usbDevicesSocketPath?: string;
  /** USB devices socket client for startup snapshots. */
  usbDevicesClient: UsbDevicesClient;
  /** HiFi serial socket client for startup snapshots. */
  hifiSerialClient: HifiSerialClient;
  /** PCM router socket client for startup snapshots. */
  pcmRouterClient: PcmRouterClient;
  /** Metadata socket client for startup snapshots. */
  metadataClient: MetadataClient;
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
    hifiSerialSocketPath,
    pcmRouterSocketPath,
    metadataSocketPath,
    usbDevicesSocketPath,
    usbDevicesClient,
    hifiSerialClient,
    pcmRouterClient,
    metadataClient: _metadataClient,
  } = options;
  void _metadataClient;

  const getStatus = () => statusStore.getStatus();
  const broadcaster = new EventBroadcaster(logger, getStatus);
  const sseHandler = new SseHandler(logger, broadcaster);
  const wsHandler = new WsHandler(logger, getStatus);
  const coordinator = new StatusUpdateCoordinator({
    statusStore,
    broadcaster,
    wsHandler,
  });

  const bridgeState = {
    usbDevices: false,
    hifiSerial: false,
    pcmRouter: false,
    metadata: false,
  };

  const journalServiceStatusBridge = new JournalServiceStatusBridge({
    logger,
    coordinator,
  });

  const usbEventBridge = usbDevicesSocketPath
    ? new UsbDevicesEventBridge({
        socketPath: usbDevicesSocketPath,
        logger,
        broadcaster,
        coordinator,
        onConnectionChange: (connected) => {
          bridgeState.usbDevices = connected;
        },
      })
    : undefined;

  const hifiEventBridge = hifiSerialSocketPath
    ? new HifiSerialEventBridge({
        socketPath: hifiSerialSocketPath,
        logger,
        broadcaster,
        coordinator,
        onConnectionChange: (connected) => {
          bridgeState.hifiSerial = connected;
        },
      })
    : undefined;

  const pcmEventBridge = pcmRouterSocketPath
    ? new PcmRouterEventBridge({
        socketPath: pcmRouterSocketPath,
        logger,
        broadcaster,
        coordinator,
        onConnectionChange: (connected) => {
          bridgeState.pcmRouter = connected;
        },
      })
    : undefined;

  const metadataEventBridge = metadataSocketPath
    ? new MetadataEventBridge({
        socketPath: metadataSocketPath,
        logger,
        broadcaster,
        coordinator,
        onConnectionChange: (connected) => {
          bridgeState.metadata = connected;
        },
      })
    : undefined;

  broadcaster.start();

  usbEventBridge?.start();
  hifiEventBridge?.start();
  pcmEventBridge?.start();
  metadataEventBridge?.start();

  const journalLogBridge = new JournalLogBridge({
    logger,
    broadcaster,
    serviceStatusBridge: journalServiceStatusBridge,
  });
  journalLogBridge.start();

  const fallbackSettings = resolveFallbackReconciliation(config);
  const fallbackReconciliation = new FallbackReconciliation({
    logger,
    coordinator,
    statusStore,
    usbDevicesClient,
    hifiSerialClient,
    getBridgeState: () => ({
      usbDevices: usbEventBridge?.isConnected() ?? false,
      hifiSerial: hifiEventBridge?.isConnected() ?? false,
      pcmRouter: pcmEventBridge?.isConnected() ?? false,
      metadata: metadataEventBridge?.isConnected() ?? false,
    }),
    intervalMs: fallbackSettings.intervalMs,
  });

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

    const loaders = createStartupSnapshotLoaders({
      coordinator,
      usbDevicesClient,
      hifiSerialClient,
      pcmRouterClient,
    });
    void loadStartupSnapshots(loaders, logger).then(() => {
      if (fallbackSettings.enabled) {
        fallbackReconciliation.start();
      }
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
    fallbackReconciliation.stop();
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

    if (audioRoutes?.matches(url.pathname)) {
      void audioRoutes.handle(req, res, url.pathname, correlationId);
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/health") {
      handleHealth(res, correlationId);
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/runtime/status") {
      handleRuntimeStatus(res, correlationId);
      return;
    }

    if (req.method === "GET" && url.pathname === "/api/core/status") {
      handleCoreStatus(res, correlationId);
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
   * Returns backend health using core/health and core/api envelopes.
   */
  function handleHealth(res: ServerResponse, correlationId: string): void {
    const system = statusStore.getStatus();
    const checks = [
      {
        name: "http",
        status: "pass" as const,
        message: "Backend listening",
      },
      {
        name: "config",
        status: system.config === "loaded" ? ("pass" as const) : ("fail" as const),
        message: `Config ${system.config}`,
      },
      {
        name: "core-platform",
        status:
          system.backend === "healthy"
            ? ("pass" as const)
            : system.backend === "degraded"
              ? ("warn" as const)
              : ("fail" as const),
        message: `Platform ${system.backend}`,
      },
    ];

    const report = createHealthReport({
      service: config.service,
      checks,
    });

    sendJson(
      res,
      200,
      createSuccessResponse({
        correlationId,
        data: report as unknown as Record<string, unknown>,
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
   * Returns aggregated core service status.
   */
  function handleCoreStatus(res: ServerResponse, correlationId: string): void {
    const payload = buildCoreStatusPayload(config, statusStore.getStatus());
    sendJson(
      res,
      200,
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
