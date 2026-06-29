import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { loadServiceConfig } from "@homepi/core-config";
import { createLogger, createCorrelationId } from "@homepi/core-logging";
import { createHttpServer } from "./http-server.js";
import { SystemStatusStore } from "./system-status-store.js";
import { UsbDevicesClient } from "./usb-devices/usb-devices-client.js";
import { UsbDevicesRoutes } from "./usb-devices/usb-devices-routes.js";
import { HifiSerialClient } from "./hifi-serial/hifi-serial-client.js";
import { HifiSerialRoutes } from "./hifi-serial/hifi-serial-routes.js";
import { AudioRoutes } from "./audio/audio-routes.js";
import { ShairportRemoteClient } from "./audio/shairport-remote-client.js";
import { MetadataClient } from "./metadata/metadata-client.js";
import { PcmRouterClient } from "./pcm-router/pcm-router-client.js";
import { AudioBrokerSnapshotStore } from "./audio/audio-broker-snapshot-store.js";
import { PagingClient } from "./audio/paging/paging-client.js";
import { PagingRoutes } from "./audio/paging/paging-routes.js";
import { PagingApiKeyRoutes } from "./audio/paging/paging-api-key-routes.js";
import { resolveRuntimeSocketPaths } from "./runtime-socket-paths.js";
import { HealthClient } from "./health/health-client.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
const configPath = join(__dirname, "..", "config", "service-config.json");

const startedAt = new Date();
let configLoaded = false;
let serviceConfig;

try {
  serviceConfig = loadServiceConfig({ configPath });
  configLoaded = true;
} catch (error) {
  const fallbackConfigPath = configPath;
  throw error instanceof Error
    ? new Error(`Failed to load config at ${fallbackConfigPath}: ${error.message}`)
    : error;
}

const logger = createLogger({
  service: configLoaded ? serviceConfig.service : "homepi-backend",
  minLevel: configLoaded ? serviceConfig.logging.level : "INFO",
});

const statusStore = new SystemStatusStore(
  {
    uptimeMs: 0,
    cpuTempC: null,
    lastEventAt: null,
  },
  startedAt
);

const socketPaths = resolveRuntimeSocketPaths(serviceConfig.runtime.paths.socketDir);
const usbSocketPath = socketPaths.usbDevices;
const usbDevicesClient = new UsbDevicesClient({ socketPath: usbSocketPath });
const usbRoutes = new UsbDevicesRoutes({ client: usbDevicesClient, logger });

const hifiSocketPath = socketPaths.hifiSerial;
const pcmRouterSocketPath = socketPaths.pcmRouter;
const metadataSocketPath = socketPaths.metadata;
const audioRealtimeSocketPath = socketPaths.audioRealtime;
const eventsBrokerSocketPath = socketPaths.broker;
const pagingSocketPath = socketPaths.paging;
const hifiSerialClient = new HifiSerialClient({ socketPath: hifiSocketPath });
const hifiRoutes = new HifiSerialRoutes({ client: hifiSerialClient, logger });
const pcmRouterClient = new PcmRouterClient({ socketPath: pcmRouterSocketPath });
const metadataClient = new MetadataClient({ socketPath: metadataSocketPath });
const shairportRemoteClient = new ShairportRemoteClient();
const brokerSnapshotStore = new AudioBrokerSnapshotStore();
const pagingClient = new PagingClient({ socketPath: pagingSocketPath });
const pagingRoutes = new PagingRoutes({
  client: pagingClient,
  logger,
  eventsSocketPath: eventsBrokerSocketPath,
});
const pagingApiKeyRoutes = new PagingApiKeyRoutes({
  client: pagingClient,
  logger,
});
const healthClient = new HealthClient(socketPaths.health);
const audioRoutes = new AudioRoutes({
  client: hifiSerialClient,
  pcmClient: pcmRouterClient,
  metadataClient,
  shairportRemote: shairportRemoteClient,
  healthClient,
  config: serviceConfig,
  logger,
  brokerSnapshotStore,
});

const host = "127.0.0.1";
const port = 3000;

logger.info({
  module: "app.backend",
  event: "lifecycle_starting",
  correlationId: createCorrelationId("startup"),
  message: "HomePi backend lifecycle starting",
  data: { environment: serviceConfig.environment },
});

const server = createHttpServer({
  config: serviceConfig,
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
  hifiSerialSocketPath: hifiSocketPath,
  pcmRouterSocketPath,
  metadataSocketPath,
  audioRealtimeSocketPath,
  eventsBrokerSocketPath: socketPaths.broker,
  brokerSocketPath: socketPaths.broker,
  healthSocketPath: socketPaths.health,
  usbDevicesSocketPath: usbSocketPath,
  metadataClient,
  brokerSnapshotStore,
});

process.on("SIGINT", () => {
  logger.info({
    module: "app.backend",
    event: "lifecycle_stopping",
    correlationId: createCorrelationId("shutdown"),
    message: "HomePi backend shutting down",
  });
  server.close(() => process.exit(0));
});
