/**
 * Runtime path configuration per runtime-config.schema.json.
 */
export interface RuntimePaths {
  homepiRoot: string;
  runtimeDir: string;
  generatedDir: string;
  cacheDir?: string;
  stateDir?: string;
  socketDir: string;
}

/**
 * Runtime configuration section.
 */
export interface RuntimeConfig {
  paths: RuntimePaths;
  watchdog?: {
    enabled?: boolean;
    timeoutMs?: number;
  };
  service?: {
    restartPolicy?: "no" | "on-failure" | "always";
    restartSec?: number;
  };
}

/**
 * Logging section of service configuration.
 */
export interface ServiceLoggingConfig {
  level: "DEBUG" | "INFO" | "WARN" | "ERROR";
  debugTrace?: boolean;
}

/**
 * Fallback reconciliation settings for event-driven service status.
 */
export interface FallbackReconciliationConfig {
  /** Whether slow fallback reconciliation runs. */
  enabled?: boolean;
  /** Reconciliation interval in milliseconds (60s–300s). */
  intervalMs?: number;
}

/**
 * Service status configuration (backend).
 */
export interface ServiceStatusConfig {
  /** Slow fallback reconciliation for fault recovery. */
  fallbackReconciliation?: FallbackReconciliationConfig;
}

/**
 * Base service configuration per service-config.schema.json.
 */
export interface ServiceConfig {
  service: string;
  version?: string;
  environment: "development" | "staging" | "production" | "test";
  enabled?: boolean;
  logging: ServiceLoggingConfig;
  runtime: RuntimeConfig;
  /** Optional status pipeline settings. */
  status?: ServiceStatusConfig;
  modules?: Record<
    string,
    {
      enabled: boolean;
      configPath: string;
    }
  >;
}

/**
 * Options for loading configuration.
 */
export interface LoadConfigOptions {
  /** Path to service JSON config file. */
  configPath?: string;
  /** Path to .env file. */
  envPath?: string;
  /** Inline environment variable overrides. */
  env?: Record<string, string>;
  /** Runtime override JSON path. */
  overridePath?: string;
  /** Default runtime config values. */
  defaults?: Partial<RuntimeConfig>;
}
