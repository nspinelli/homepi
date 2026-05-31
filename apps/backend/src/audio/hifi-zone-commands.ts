/**
 * Escapes a string for Hi-Fi2 quoted protocol values.
 * @param value - Raw string.
 * @returns Escaped value without surrounding quotes.
 */
function escapeProtocolString(value: string): string {
  return value.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\*/g, "\\*");
}

/**
 * Builds a Hi-Fi2 zone name SET command.
 * @param zone - Zone number 1-16.
 * @param name - Zone display name.
 * @returns Command string including leading *.
 */
export function buildZoneNameCommand(zone: number, name: string): string {
  return `*Z${zone}NAME"${escapeProtocolString(name)}"`;
}

/**
 * Builds a Hi-Fi2 zone enable SET command.
 * @param zone - Zone number 1-16.
 * @param enabled - 0 or 1.
 * @returns Command string.
 */
export function buildZoneEnableCommand(zone: number, enabled: number): string {
  return `*Z${zone}ENABLE${enabled}`;
}

/**
 * Builds a Hi-Fi2 zone power SET command.
 * @param zone - Zone number 1-16.
 * @param power - 0 or 1.
 * @returns Command string.
 */
export function buildZonePowerCommand(zone: number, power: number): string {
  return `*Z${zone}POWER${power}`;
}

/**
 * Builds a Hi-Fi2 zone volume SET command.
 * @param zone - Zone number 1-16.
 * @param volume - Volume 0-100.
 * @returns Command string.
 */
export function buildZoneVolumeCommand(zone: number, volume: number): string {
  return `*Z${zone}VOLUME${volume}`;
}

/**
 * Builds a Hi-Fi2 zone treble SET command.
 * @param zone - Zone number.
 * @param treble - Treble value.
 * @returns Command string.
 */
export function buildZoneTrebleCommand(zone: number, treble: number): string {
  return `*Z${zone}TREB${treble}`;
}

/**
 * Builds a Hi-Fi2 zone bass SET command.
 * @param zone - Zone number.
 * @param bass - Bass value.
 * @returns Command string.
 */
export function buildZoneBassCommand(zone: number, bass: number): string {
  return `*Z${zone}BASS${bass}`;
}

/**
 * Builds a Hi-Fi2 zone balance SET command.
 * @param zone - Zone number.
 * @param balance - Balance value.
 * @returns Command string.
 */
export function buildZoneBalanceCommand(zone: number, balance: number): string {
  return `*Z${zone}BAL${balance}`;
}

/**
 * Builds a Hi-Fi2 zone loudness SET command.
 * @param zone - Zone number.
 * @param loudness - Loudness value.
 * @returns Command string.
 */
export function buildZoneLoudnessCommand(zone: number, loudness: number): string {
  return `*Z${zone}LOUDNESS${loudness}`;
}

/**
 * Builds a Hi-Fi2 zone initial volume SET command.
 * @param zone - Zone number.
 * @param volume - Initial volume 0-100.
 * @returns Command string.
 */
export function buildZoneInitialVolumeCommand(zone: number, volume: number): string {
  return `*Z${zone}INIVOL${volume}`;
}

/**
 * Builds a Hi-Fi2 zone page volume SET command.
 * @param zone - Zone number.
 * @param volume - Page volume 0-100.
 * @returns Command string.
 */
export function buildZonePageVolumeCommand(zone: number, volume: number): string {
  return `*Z${zone}PGVOL${volume}`;
}

/**
 * Builds a Hi-Fi2 zone group SET command.
 * @param zone - Zone number.
 * @param group - Group number.
 * @returns Command string.
 */
export function buildZoneGroupCommand(zone: number, group: number): string {
  return `*Z${zone}GROUP${group}`;
}

/**
 * Controller zone fields that can be sent to the Hi-Fi2 device.
 */
export interface ZoneControllerPatch {
  name?: string;
  enabled?: number;
  power?: number;
  volume?: number;
  treble?: number;
  bass?: number;
  balance?: number;
  loudness?: number;
  initialVolume?: number;
  pageVolume?: number;
  groupNumber?: number;
}

/**
 * Builds Hi-Fi2 SET commands for changed controller fields.
 * @param zone - Zone number 1-16.
 * @param patch - Fields to update.
 * @returns Command strings to queue on the serial service.
 */
export function buildZoneControllerCommands(
  zone: number,
  patch: ZoneControllerPatch
): string[] {
  const commands: string[] = [];
  if (patch.name !== undefined) {
    commands.push(buildZoneNameCommand(zone, patch.name));
  }
  if (patch.enabled !== undefined) {
    commands.push(buildZoneEnableCommand(zone, patch.enabled));
  }
  if (patch.power !== undefined) {
    commands.push(buildZonePowerCommand(zone, patch.power));
  }
  if (patch.volume !== undefined) {
    commands.push(buildZoneVolumeCommand(zone, patch.volume));
  }
  if (patch.treble !== undefined) {
    commands.push(buildZoneTrebleCommand(zone, patch.treble));
  }
  if (patch.bass !== undefined) {
    commands.push(buildZoneBassCommand(zone, patch.bass));
  }
  if (patch.balance !== undefined) {
    commands.push(buildZoneBalanceCommand(zone, patch.balance));
  }
  if (patch.loudness !== undefined) {
    commands.push(buildZoneLoudnessCommand(zone, patch.loudness));
  }
  if (patch.initialVolume !== undefined) {
    commands.push(buildZoneInitialVolumeCommand(zone, patch.initialVolume));
  }
  if (patch.pageVolume !== undefined) {
    commands.push(buildZonePageVolumeCommand(zone, patch.pageVolume));
  }
  if (patch.groupNumber !== undefined) {
    commands.push(buildZoneGroupCommand(zone, patch.groupNumber));
  }
  return commands;
}

/**
 * Returns true when changed fields affect generated Shairport config hash.
 * @param patch - Controller patch.
 * @param shairport - Shairport settings patch.
 * @returns Whether supervisor should restart the zone.
 */
export function requiresShairportRestart(
  patch: ZoneControllerPatch,
  shairport: ShairportZonePatch
): boolean {
  return (
    patch.name !== undefined ||
    patch.initialVolume !== undefined ||
    shairport.volumeControlProfile !== undefined ||
    shairport.activeStateTimeout !== undefined ||
    shairport.sessionTimeout !== undefined ||
    shairport.logVerbosity !== undefined
  );
}

/**
 * Shairport per-zone settings patch.
 */
export interface ShairportZonePatch {
  volumeControlProfile?: string;
  activeStateTimeout?: number;
  sessionTimeout?: number;
  logVerbosity?: number;
}
