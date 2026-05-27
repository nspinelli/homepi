import { readFileSync, existsSync } from "node:fs";
import { createLogger, type Logger } from "@homepi/core-logging";
import type { LoadConfigOptions, RuntimeConfig, ServiceConfig } from "./config-types.js";
import { loadEnvFile, mergeEnv, type EnvMap } from "./env-loader.js";
import { mergeRuntimeConfig } from "./merge-config.js";
import { normalizeRuntimeConfig, normalizeServiceConfig } from "./normalize-config.js";
import { validateServiceConfig } from "./validate-config.js";

/**
 * Reads and parses a JSON file.
 * @param path - File path.
 * @returns Parsed JSON value.
 */
function readJsonFile(path: string): unknown {
  return JSON.parse(readFileSync(path, "utf8")) as unknown;
}

/**
 * Applies documented environment variable overrides to service config.
 * @param config - Base service config.
 * @param env - Environment map.
 * @returns Config with env overrides applied.
 */
function applyEnvOverrides(config: ServiceConfig, env: EnvMap): ServiceConfig {
  const updated = JSON.parse(JSON.stringify(config)) as ServiceConfig;

  if (env.LOG_LEVEL) {
    const level = env.LOG_LEVEL as ServiceConfig["logging"]["level"];
    if (["DEBUG", "INFO", "WARN", "ERROR"].includes(level)) {
      updated.logging.level = level;
    }
  }

  if (env.HOMEPI_ROOT) {
    updated.runtime.paths.homepiRoot = env.HOMEPI_ROOT;
  }
  if (env.HOMEPI_RUNTIME_DIR) {
    updated.runtime.paths.runtimeDir = env.HOMEPI_RUNTIME_DIR;
  }
  if (env.HOMEPI_SOCKET_DIR) {
    updated.runtime.paths.socketDir = env.HOMEPI_SOCKET_DIR;
  }

  return updated;
}

/**
 * Loads, merges, validates, and normalizes HomePi service configuration.
 * @param options - Load options.
 * @returns Typed, validated service configuration.
 * @throws When validation fails.
 */
export function loadServiceConfig(options: LoadConfigOptions = {}): ServiceConfig {
  const correlationId = "startup-config-load";
  const logger: Logger = createLogger({
    service: "homepi-config-loader",
    minLevel: "INFO",
  });

  let config: ServiceConfig | undefined;
  const envLayers: EnvMap[] = [];

  if (options.envPath) {
    const loaded = loadEnvFile(options.envPath);
    envLayers.push(loaded);
    logger.info({
      module: "core.config",
      event: "env_loaded",
      correlationId,
      message: "Environment file loaded",
      data: { envPath: options.envPath, keys: Object.keys(loaded).length },
    });
  }

  if (options.env) {
    const upper: EnvMap = {};
    for (const [k, v] of Object.entries(options.env)) {
      upper[k.toUpperCase()] = v;
    }
    envLayers.push(upper);
  }

  const env = mergeEnv(...envLayers);

  if (options.configPath && existsSync(options.configPath)) {
    config = readJsonFile(options.configPath) as ServiceConfig;
  }

  if (!config) {
    throw new Error("Service configuration file is required");
  }

  let runtimeLayers: Partial<RuntimeConfig>[] = [options.defaults ?? {}, config.runtime];

  if (options.overridePath && existsSync(options.overridePath)) {
    runtimeLayers.push(readJsonFile(options.overridePath) as Partial<RuntimeConfig>);
    logger.info({
      module: "core.config",
      event: "runtime_override_applied",
      correlationId,
      message: "Runtime override applied",
      data: { overridePath: options.overridePath },
    });
  }

  config.runtime = mergeRuntimeConfig(...runtimeLayers);
  config = applyEnvOverrides(config, env);

  const validation = validateServiceConfig(config);
  if (!validation.valid) {
    logger.error({
      module: "core.config",
      event: "config_invalid",
      correlationId: "startup-config-validation",
      message: "Configuration validation failed",
      data: { errors: validation.errors },
    });
    throw new Error("Configuration validation failed");
  }

  const normalized = normalizeServiceConfig(validation.value!);

  logger.info({
    module: "core.config",
    event: "config_loaded",
    correlationId,
    message: "Configuration loaded successfully",
    data: {
      environment: normalized.environment,
      configPath: options.configPath,
    },
  });

  return normalized;
}

/**
 * Returns default runtime configuration paths.
 * @returns Default runtime config.
 */
export function getDefaultRuntimeConfig(): RuntimeConfig {
  return normalizeRuntimeConfig({});
}
