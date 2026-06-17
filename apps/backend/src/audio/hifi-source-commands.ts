/**
 * Escapes a string for Hi-Fi2 quoted protocol values.
 * @param value - Raw string.
 * @returns Escaped value without surrounding quotes.
 */
function escapeProtocolString(value: string): string {
  return value.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\*/g, "\\*");
}

/**
 * Builds a Hi-Fi2 source name SET command.
 * @param source - Source number 1-8.
 * @param name - Source display name.
 * @returns Command string including leading *.
 */
export function buildSourceNameCommand(source: number, name: string): string {
  return `*S${source}NAME"${escapeProtocolString(name)}"`;
}

/**
 * Builds a Hi-Fi2 source enable SET command.
 * @param source - Source number 1-8.
 * @param enabled - 0 or 1.
 * @returns Command string.
 */
export function buildSourceEnableCommand(source: number, enabled: number): string {
  return `*S${source}ENABLE${enabled}`;
}

/**
 * Builds a Hi-Fi2 source input gain SET command.
 * @param source - Source number 1-8.
 * @param inputGain - Input gain value.
 * @returns Command string.
 */
export function buildSourceInputGainCommand(source: number, inputGain: number): string {
  return `*S${source}INGAIN${inputGain}`;
}

/**
 * Builds a Hi-Fi2 source display line SET command.
 * @param source - Source number 1-8.
 * @param displayLine - Display line text.
 * @returns Command string.
 */
export function buildSourceDisplayLineCommand(source: number, displayLine: string): string {
  return `*S${source}DISPLINE"${escapeProtocolString(displayLine)}"`;
}

/**
 * Controller source fields that can be sent to the Hi-Fi2 device.
 */
export interface SourceControllerPatch {
  name?: string;
  enabled?: number;
  inputGain?: number;
  displayLine?: string;
}

/**
 * Builds Hi-Fi2 SET commands for changed controller source fields.
 * @param source - Source number 1-8.
 * @param patch - Fields to update.
 * @returns Command strings to queue on the serial service.
 */
export function buildSourceControllerCommands(
  source: number,
  patch: SourceControllerPatch
): string[] {
  const commands: string[] = [];
  if (patch.name !== undefined) {
    commands.push(buildSourceNameCommand(source, patch.name));
  }
  if (patch.enabled !== undefined) {
    commands.push(buildSourceEnableCommand(source, patch.enabled));
  }
  if (patch.inputGain !== undefined) {
    commands.push(buildSourceInputGainCommand(source, patch.inputGain));
  }
  if (patch.displayLine !== undefined) {
    commands.push(buildSourceDisplayLineCommand(source, patch.displayLine));
  }
  return commands;
}
