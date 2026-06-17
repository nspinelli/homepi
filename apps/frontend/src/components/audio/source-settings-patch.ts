import { isSourceEnabled } from "@/lib/is-source-enabled.js";
import type { HifiSource, SourceSettingsPatch } from "@/types/audio-types.js";

/**
 * Local form state for the source settings editor.
 */
export interface SourceSettingsFormState {
  name: string;
  enabled: number;
  inputGain: number;
  displayLine: string;
  isAirplay: boolean;
}

/**
 * Stable key for a source row used to detect snapshot updates.
 * @param source - Hi-Fi source row.
 * @returns Serialized snapshot fingerprint.
 */
export function sourceSettingsSnapshotKey(source: HifiSource): string {
  return JSON.stringify({
    sourceNumber: source.sourceNumber,
    name: source.name,
    enabled: source.enabled,
    inputGain: source.inputGain,
    displayLine: source.displayLine,
    isAirplay: source.isAirplay,
  });
}

/**
 * Builds form state from a source snapshot row.
 * @param source - Hi-Fi source row.
 * @returns Initial form values.
 */
export function sourceToFormState(source: HifiSource): SourceSettingsFormState {
  return {
    name: source.name ?? `Source ${source.sourceNumber}`,
    enabled: isSourceEnabled(source) ? 1 : 0,
    inputGain: source.inputGain ?? 0,
    displayLine: source.displayLine ?? "",
    isAirplay: source.isAirplay === 1,
  };
}

/**
 * Returns true when form values differ from the persisted snapshot.
 * @param source - Hi-Fi source row.
 * @param form - Current form state.
 * @returns Whether the user has unsaved edits.
 */
export function hasSourceSettingsChanges(
  source: HifiSource,
  form: SourceSettingsFormState
): boolean {
  return buildSourceSettingsPatch(source, form) !== null;
}

/**
 * Builds a diff-only patch for changed fields, or null when nothing changed.
 * @param source - Hi-Fi source row.
 * @param form - Current form state.
 * @returns Patch payload or null.
 */
export function buildSourceSettingsPatch(
  source: HifiSource,
  form: SourceSettingsFormState
): SourceSettingsPatch | null {
  const patch: SourceSettingsPatch = {
    controller: {},
  };

  if (form.name !== (source.name ?? "")) {
    patch.controller!.name = form.name;
  }
  if (form.enabled !== (isSourceEnabled(source) ? 1 : 0)) {
    patch.controller!.enabled = form.enabled;
  }
  if (form.inputGain !== (source.inputGain ?? 0)) {
    patch.controller!.inputGain = form.inputGain;
  }
  if (form.displayLine !== (source.displayLine ?? "")) {
    patch.controller!.displayLine = form.displayLine;
  }
  if (form.isAirplay && source.isAirplay !== 1) {
    patch.airplay = true;
  }

  const controllerKeys = Object.keys(patch.controller ?? {}).length;
  const hasAirplayChange = patch.airplay === true;

  if (controllerKeys === 0 && !hasAirplayChange) {
    return null;
  }

  if (controllerKeys === 0) {
    delete patch.controller;
  }

  return patch;
}
