import { readFile } from "node:fs/promises";
import { execFile } from "node:child_process";
import { promisify } from "node:util";

const execFileAsync = promisify(execFile);

/**
 * Reads the Raspberry Pi CPU temperature in degrees Celsius.
 * @returns Temperature in °C, or null when unavailable.
 */
export async function readCpuTemperatureC(): Promise<number | null> {
  try {
    const raw = await readFile("/sys/class/thermal/thermal_zone0/temp", "utf8");
    const milliC = Number.parseInt(raw.trim(), 10);
    if (Number.isNaN(milliC)) {
      return null;
    }
    return Math.round(milliC / 100) / 10;
  } catch {
    try {
      const { stdout } = await execFileAsync("vcgencmd", ["measure_temp"]);
      const match = stdout.match(/temp=([0-9.]+)/);
      if (!match) {
        return null;
      }
      const value = Number.parseFloat(match[1] ?? "");
      return Number.isNaN(value) ? null : Math.round(value * 10) / 10;
    } catch {
      return null;
    }
  }
}
