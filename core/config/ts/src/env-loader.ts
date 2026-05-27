import { readFileSync, existsSync } from "node:fs";

/**
 * Parsed environment variable map (uppercase keys).
 */
export type EnvMap = Record<string, string>;

/**
 * Loads environment variables from a .env file without mutating process.env.
 * @param envPath - Path to .env file.
 * @returns Parsed environment map.
 */
export function loadEnvFile(envPath: string): EnvMap {
  if (!existsSync(envPath)) {
    return {};
  }

  const content = readFileSync(envPath, "utf8");
  const result: EnvMap = {};

  for (const line of content.split("\n")) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith("#")) {
      continue;
    }
    const eq = trimmed.indexOf("=");
    if (eq === -1) {
      continue;
    }
    const key = trimmed.slice(0, eq).trim();
    let value = trimmed.slice(eq + 1).trim();
    if (
      (value.startsWith('"') && value.endsWith('"')) ||
      (value.startsWith("'") && value.endsWith("'"))
    ) {
      value = value.slice(1, -1);
    }
    result[key.toUpperCase()] = value;
  }

  return result;
}

/**
 * Merges environment sources; later sources override earlier ones.
 * @param sources - Environment maps in precedence order (lowest first).
 * @returns Merged environment map.
 */
export function mergeEnv(...sources: EnvMap[]): EnvMap {
  return Object.assign({}, ...sources);
}
