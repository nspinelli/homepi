import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { loadServiceConfig } from "@homepi/core-config";
import { createLogger, createCorrelationId } from "@homepi/core-logging";
import { createHttpServer } from "./http-server.js";
import { SystemStatusStore } from "./system-status-store.js";
import { UsbDevicesClient } from "./usb-devices/usb-devices-client.js";
import { UsbDevicesRoutes } from "./usb-devices/usb-devices-routes.js";
import type {
  HifiSerialStatus,
  MetadataStatus,
  NqptpStatus,
  PcmRouterStatus,
  ShairportStatus,
  UsbDevicesStatus,
} from "./types/system-status-types.js";
import { getSystemdUnitActiveState } from "./runtime/check-systemd-unit.js";
import { HifiSerialClient } from "./hifi-serial/hifi-serial-client.js";
import { HifiSerialRoutes } from "./hifi-serial/hifi-serial-routes.js";
import { AudioRoutes } from "./audio/audio-routes.js";
import { readCpuTemperatureC } from "./system/read-cpu-temperature.js";
import type { HifiSerialHealth } from "./hifi-serial/hifi-serial-types.js";

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

const statusStore = new SystemStatusStore({
  backend: "healthy",
  config: configLoaded ? "loaded" : "invalid",
  logging: "active",
  runtime: "running",
  transport: "ready",
  events: "ready",
  state: "ready",
  api: "ready",
  usbDevices: "offline",
  hifiSerial: "offline",
  nqptp: "offline",
  metadata: "offline",
  pcmRouter: "offline",
  shairport: "offline",
  uptimeMs: 0,
  cpuTempC: null,
  lastEventAt: null,
});

const usbSocketPath = `${serviceConfig.runtime.paths.socketDir}/usb-devices.sock`;
const usbDevicesClient = new UsbDevicesClient({ socketPath: usbSocketPath });
const usbRoutes = new UsbDevicesRoutes({ client: usbDevicesClient, logger });

const hifiSocketPath = `${serviceConfig.runtime.paths.socketDir}/hifi-serial.sock`;
const pcmRouterSocketPath = `${serviceConfig.runtime.paths.socketDir}/pcm-router.sock`;
const hifiSerialClient = new HifiSerialClient({ socketPath: hifiSocketPath });
const hifiRoutes = new HifiSerialRoutes({ client: hifiSerialClient, logger });
const audioRoutes = new AudioRoutes({ client: hifiSerialClient, logger });

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
  hifiSerialSocketPath: hifiSocketPath,
  pcmRouterSocketPath,
});

/**
 * Maps native USB health to dashboard status.
 * @param reachable - Socket reachable flag.
 * @param degraded - Assignments degraded flag.
 * @returns Dashboard usbDevices status.
 */
function mapUsbDevicesStatus(reachable: boolean, degraded: boolean): UsbDevicesStatus {
  if (!reachable) {
    return "offline";
  }
  return degraded ? "degraded" : "healthy";
}

/**
 * Polls the native USB service and updates the system status store.
 */
async function pollUsbDevicesHealth(): Promise<void> {
  const correlationId = createCorrelationId("usb-health");
  try {
    const health = await usbDevicesClient.getHealth(correlationId);
    statusStore.patchStatus({
      usbDevices: mapUsbDevicesStatus(true, health.assignmentsDegraded),
    });
  } catch {
    statusStore.patchStatus({ usbDevices: "offline" });
  }
}

/**
 * Maps native HiFi health to dashboard status.
 * @param health - Native health snapshot.
 * @returns Dashboard hifiSerial status.
 */
function mapHifiSerialStatus(health: HifiSerialHealth): HifiSerialStatus {
  if (!health.connected) {
    return "offline";
  }
  if (health.degraded || health.syncInProgress) {
    return "degraded";
  }
  return "healthy";
}

/**
 * Maps systemd active state to dashboard external service status.
 * @param state - systemctl is-active output.
 * @returns Dashboard healthy / degraded / offline.
 */
function mapSystemdServiceStatus(
  state: string
): NqptpStatus | MetadataStatus | PcmRouterStatus | ShairportStatus {
  if (state === "active") {
    return "healthy";
  }
  if (state === "activating" || state === "reloading") {
    return "degraded";
  }
  return "offline";
}

/**
 * Polls homepi-nqptp via systemd and updates the system status store.
 */
async function pollNqptpHealth(): Promise<void> {
  try {
    const state = await getSystemdUnitActiveState("homepi-nqptp");
    statusStore.patchStatus({ nqptp: mapSystemdServiceStatus(state) });
  } catch {
    statusStore.patchStatus({ nqptp: "offline" });
  }
}

/**
 * Polls homepi-metadata via systemd and updates the system status store.
 */
async function pollMetadataHealth(): Promise<void> {
  try {
    const supervisor = await getSystemdUnitActiveState("homepi-shairport-supervisor");
    if (supervisor !== "active") {
      statusStore.patchStatus({ metadata: "offline" });
      return;
    }
    const zone1 = await getSystemdUnitActiveState("homepi-metadata@1");
    statusStore.patchStatus({ metadata: mapSystemdServiceStatus(zone1) });
  } catch {
    statusStore.patchStatus({ metadata: "offline" });
  }
}

/**
 * Polls homepi-shairport-supervisor via systemd and updates the system status store.
 */
async function pollShairportHealth(): Promise<void> {
  try {
    const state = await getSystemdUnitActiveState("homepi-shairport-supervisor");
    statusStore.patchStatus({ shairport: mapSystemdServiceStatus(state) });
  } catch {
    statusStore.patchStatus({ shairport: "offline" });
  }
}

/**
 * Polls homepi-pcm-router via systemd and updates the system status store.
 */
async function pollPcmRouterHealth(): Promise<void> {
  try {
    const state = await getSystemdUnitActiveState("homepi-pcm-router");
    statusStore.patchStatus({ pcmRouter: mapSystemdServiceStatus(state) });
  } catch {
    statusStore.patchStatus({ pcmRouter: "offline" });
  }
}

/**
 * Polls the native HiFi serial service and updates the system status store.
 */
async function pollHifiSerialHealth(): Promise<void> {
  const correlationId = createCorrelationId("hifi-health");
  try {
    const health = await hifiSerialClient.getHealth(correlationId);
    statusStore.patchStatus({
      hifiSerial: mapHifiSerialStatus(health),
    });
  } catch {
    statusStore.patchStatus({ hifiSerial: "offline" });
  }
}

/**
 * Polls CPU temperature and updates the system status store.
 */
async function pollCpuTemperature(): Promise<void> {
  const cpuTempC = await readCpuTemperatureC();
  statusStore.patchStatus({ cpuTempC });
}

void pollUsbDevicesHealth();
void pollHifiSerialHealth();
void pollNqptpHealth();
void pollMetadataHealth();
void pollShairportHealth();
void pollPcmRouterHealth();
void pollCpuTemperature();
setInterval(() => {
  void pollUsbDevicesHealth();
}, 10_000);
setInterval(() => {
  void pollHifiSerialHealth();
}, 10_000);
setInterval(() => {
  void pollNqptpHealth();
}, 10_000);
setInterval(() => {
  void pollMetadataHealth();
}, 10_000);
setInterval(() => {
  void pollShairportHealth();
}, 10_000);
setInterval(() => {
  void pollPcmRouterHealth();
}, 10_000);
setInterval(() => {
  void pollCpuTemperature();
}, 5_000);

setInterval(() => {
  statusStore.patchStatus({
    uptimeMs: Math.max(0, Date.now() - startedAt.getTime()),
  });
}, 1_000);

process.on("SIGINT", () => {
  logger.info({
    module: "app.backend",
    event: "lifecycle_stopping",
    correlationId: createCorrelationId("shutdown"),
    message: "HomePi backend shutting down",
  });
  server.close(() => process.exit(0));
});
