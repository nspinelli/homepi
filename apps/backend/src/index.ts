import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { loadServiceConfig } from "@homepi/core-config";
import { createLogger, createCorrelationId } from "@homepi/core-logging";
import { createHttpServer } from "./http-server.js";
import { SystemStatusStore } from "./system-status-store.js";

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
  uptimeMs: 0,
  lastEventAt: null,
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
});

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
