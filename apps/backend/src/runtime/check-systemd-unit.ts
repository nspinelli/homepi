import { spawn } from "node:child_process";

const DEFAULT_TIMEOUT_MS = 3000;

/**
 * Runs systemctl is-active for a unit and returns the trimmed stdout state.
 * @param unitName - systemd unit name (with or without .service suffix).
 * @param timeoutMs - Max wait before rejecting.
 * @returns Active state string (e.g. active, inactive, failed, activating).
 */
export function getSystemdUnitActiveState(
  unitName: string,
  timeoutMs: number = DEFAULT_TIMEOUT_MS
): Promise<string> {
  const unit = unitName.endsWith(".service") ? unitName : `${unitName}.service`;

  return new Promise((resolve, reject) => {
    const child = spawn("systemctl", ["is-active", unit], {
      stdio: ["ignore", "pipe", "pipe"],
    });

    let stdout = "";
    let stderr = "";

    const timer = setTimeout(() => {
      child.kill("SIGTERM");
      reject(new Error(`systemctl timed out for ${unit}`));
    }, timeoutMs);

    child.stdout?.on("data", (chunk: Buffer) => {
      stdout += chunk.toString();
    });
    child.stderr?.on("data", (chunk: Buffer) => {
      stderr += chunk.toString();
    });

    child.on("error", (error) => {
      clearTimeout(timer);
      reject(error);
    });

    child.on("close", (code) => {
      clearTimeout(timer);
      const state = stdout.trim() || stderr.trim() || "unknown";
      if (code === 0) {
        resolve(state);
      } else {
        resolve(state);
      }
    });
  });
}

/**
 * Returns true when systemctl reports the unit as active.
 * @param unitName - systemd unit name.
 * @param timeoutMs - Max wait before treating as inactive.
 * @returns Whether the unit is active.
 */
export async function isSystemdUnitActive(
  unitName: string,
  timeoutMs: number = DEFAULT_TIMEOUT_MS
): Promise<boolean> {
  const state = await getSystemdUnitActiveState(unitName, timeoutMs);
  return state === "active";
}
