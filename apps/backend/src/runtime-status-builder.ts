import type { ServiceConfig } from "@homepi/core-config";

/**
 * Runtime status payload aligned with runtime-status.schema.json.
 */
export interface RuntimeStatusPayload {
  /** Service identifier. */
  service: string;
  /** Lifecycle state. */
  state: "starting" | "running" | "stopping" | "stopped" | "failed" | "restarting";
  /** Process identifier. */
  pid: number;
  /** ISO8601 process start timestamp. */
  startedAt: string;
  /** Uptime in milliseconds. */
  uptimeMs: number;
  /** Restart counter. */
  restartCount: number;
  /** Optional runtime path map from config. */
  runtimePaths?: Record<string, string>;
}

/**
 * Builds a schema-aligned runtime status payload for the backend process.
 * @param config - Loaded service configuration.
 * @param startedAt - Process start time.
 * @returns Runtime status payload.
 */
export function buildRuntimeStatusPayload(
  config: ServiceConfig,
  startedAt: Date
): RuntimeStatusPayload {
  const uptimeMs = Math.max(0, Date.now() - startedAt.getTime());
  const paths = config.runtime.paths;

  return {
    service: config.service,
    state: "running",
    pid: process.pid,
    startedAt: startedAt.toISOString(),
    uptimeMs,
    restartCount: 0,
    runtimePaths: {
      homepiRoot: paths.homepiRoot,
      runtimeDir: paths.runtimeDir,
      generatedDir: paths.generatedDir,
      ...(paths.cacheDir ? { cacheDir: paths.cacheDir } : {}),
      ...(paths.stateDir ? { stateDir: paths.stateDir } : {}),
      socketDir: paths.socketDir,
    },
  };
}
