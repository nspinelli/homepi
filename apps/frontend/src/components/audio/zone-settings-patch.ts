import { zoneInitialVolume } from "@/lib/zone-initial-volume.js";
import type { HifiZone, ShairportZoneSettings, ZoneSettingsPatch } from "@/types/audio-types.js";

/**
 * Local form state for the zone settings editor.
 */
export interface ZoneSettingsFormState {
  name: string;
  enabled: number;
  treble: number;
  bass: number;
  balance: number;
  loudness: number;
  initialVolume: number;
  pageVolume: number;
  groupNumber: number;
  volumeProfile: string;
  activeTimeout: number;
  sessionTimeout: number;
  logVerbosity: number;
}

/**
 * Stable key for zone + Shairport rows used to detect snapshot updates.
 * @param zone - Hi-Fi zone row.
 * @param shairport - Shairport settings for the zone.
 * @returns Serialized snapshot fingerprint.
 */
export function zoneSettingsSnapshotKey(
  zone: HifiZone,
  shairport: ShairportZoneSettings | null
): string {
  return JSON.stringify({
    zoneNumber: zone.zoneNumber,
    name: zone.name,
    enabled: zone.enabled,
    treble: zone.treble,
    bass: zone.bass,
    balance: zone.balance,
    loudness: zone.loudness,
    initialVolume: zone.initialVolume,
    pageVolume: zone.pageVolume,
    groupNumber: zone.groupNumber,
    volume: zone.volume,
    shairport: shairport ?? null,
  });
}

/**
 * Builds form state from a zone snapshot row and Shairport settings.
 * @param zone - Hi-Fi zone row.
 * @param shairport - Shairport settings for the zone.
 * @returns Initial form values.
 */
export function zoneToFormState(
  zone: HifiZone,
  shairport: ShairportZoneSettings | null
): ZoneSettingsFormState {
  return {
    name: zone.name ?? `Zone ${zone.zoneNumber}`,
    enabled: zone.enabled === 1 ? 1 : 0,
    treble: zone.treble ?? 0,
    bass: zone.bass ?? 0,
    balance: zone.balance ?? 0,
    loudness: zone.loudness ?? 0,
    initialVolume: zoneInitialVolume(zone),
    pageVolume: zone.pageVolume ?? 50,
    groupNumber: zone.groupNumber ?? 0,
    volumeProfile: shairport?.volumeControlProfile ?? "standard",
    activeTimeout: shairport?.activeStateTimeout ?? 5,
    sessionTimeout: shairport?.sessionTimeout ?? 60,
    logVerbosity: shairport?.logVerbosity ?? 1,
  };
}

/**
 * Returns true when form values differ from the persisted snapshot.
 * @param zone - Hi-Fi zone row.
 * @param shairport - Shairport settings for the zone.
 * @param form - Current form state.
 * @returns Whether the user has unsaved edits.
 */
export function hasZoneSettingsChanges(
  zone: HifiZone,
  shairport: ShairportZoneSettings | null,
  form: ZoneSettingsFormState
): boolean {
  return buildZoneSettingsPatch(zone, shairport, form) !== null;
}

/**
 * Builds a diff-only patch for changed fields, or null when nothing changed.
 * @param zone - Hi-Fi zone row.
 * @param shairport - Shairport settings for the zone.
 * @param form - Current form state.
 * @returns Patch payload or null.
 */
export function buildZoneSettingsPatch(
  zone: HifiZone,
  shairport: ShairportZoneSettings | null,
  form: ZoneSettingsFormState
): ZoneSettingsPatch | null {
  const patch: ZoneSettingsPatch = {
    controller: {},
    shairport: {},
  };

  if (form.name !== (zone.name ?? "")) {
    patch.controller!.name = form.name;
  }
  if (form.enabled !== (zone.enabled === 1 ? 1 : 0)) {
    patch.controller!.enabled = form.enabled;
  }
  if (form.treble !== (zone.treble ?? 0)) {
    patch.controller!.treble = form.treble;
  }
  if (form.bass !== (zone.bass ?? 0)) {
    patch.controller!.bass = form.bass;
  }
  if (form.balance !== (zone.balance ?? 0)) {
    patch.controller!.balance = form.balance;
  }
  if (form.loudness !== (zone.loudness ?? 0)) {
    patch.controller!.loudness = form.loudness;
  }
  if (form.initialVolume !== zoneInitialVolume(zone)) {
    patch.controller!.initialVolume = form.initialVolume;
  }
  if (form.pageVolume !== (zone.pageVolume ?? 50)) {
    patch.controller!.pageVolume = form.pageVolume;
  }
  if (form.groupNumber !== (zone.groupNumber ?? 0)) {
    patch.controller!.groupNumber = form.groupNumber;
  }
  if (form.volumeProfile !== (shairport?.volumeControlProfile ?? "standard")) {
    patch.shairport!.volumeControlProfile = form.volumeProfile;
  }
  if (form.activeTimeout !== (shairport?.activeStateTimeout ?? 5)) {
    patch.shairport!.activeStateTimeout = form.activeTimeout;
  }
  if (form.sessionTimeout !== (shairport?.sessionTimeout ?? 60)) {
    patch.shairport!.sessionTimeout = form.sessionTimeout;
  }
  if (form.logVerbosity !== (shairport?.logVerbosity ?? 1)) {
    patch.shairport!.logVerbosity = form.logVerbosity;
  }

  const controllerKeys = Object.keys(patch.controller ?? {}).length;
  const shairportKeys = Object.keys(patch.shairport ?? {}).length;

  if (controllerKeys === 0 && shairportKeys === 0) {
    return null;
  }

  return patch;
}

/**
 * Builds a Hi-Fi2 name command preview for staged edits.
 * @param zoneNumber - Zone number 1-16.
 * @param name - Staged zone name.
 * @returns Command string preview.
 */
export function buildZoneNameCommandPreview(zoneNumber: number, name: string): string {
  const escaped = name.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\*/g, "\\*");
  return `*Z${zoneNumber}NAME"${escaped}"`;
}
