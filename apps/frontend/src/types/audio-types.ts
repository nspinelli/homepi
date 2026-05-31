/**
 * Hi-Fi zone row from the controller database.
 */
export interface HifiZone {
  zoneNumber: number;
  name?: string;
  enabled?: number;
  treble?: number;
  bass?: number;
  balance?: number;
  loudness?: number;
  initialVolume?: number;
  pageVolume?: number;
  groupNumber?: number;
  power?: number;
  volume?: number;
  mute?: number;
  source?: number;
}

/**
 * Hi-Fi source row.
 */
export interface HifiSource {
  sourceNumber: number;
  name?: string;
  enabled?: number;
  inputGain?: number;
  displayLine?: string;
  isAirplay?: number;
}

/**
 * Hi-Fi group row.
 */
export interface HifiGroup {
  groupNumber: number;
  name?: string;
  type?: number;
}

/**
 * Hi-Fi controller row.
 */
export interface HifiController {
  pageActive?: number;
  deviceName?: string;
  lastFullSyncAt?: string;
}

/**
 * Shairport per-zone settings from SQLite.
 */
export interface ShairportZoneSettings {
  zoneNumber: number;
  volumeControlProfile?: string;
  activeStateTimeout?: number;
  sessionTimeout?: number;
  logVerbosity?: number;
}

/**
 * PCM router playback metadata for the DAC owner zone.
 */
export interface PcmMetadata {
  title?: string;
  artist?: string;
  album?: string;
  clientName?: string;
}

/**
 * PCM router live routing state.
 */
export interface PcmState {
  ownerZoneId: number;
  activeStack: number[];
  dacState: string;
  metadata: PcmMetadata;
}

/**
 * Audio service health rollup.
 */
export interface AudioServiceStatus {
  hifiSerial: string;
  shairport: string;
  pcmRouter: string;
  nqptp: string;
  metadata: string;
}

/**
 * Initial audio module snapshot from GET /api/audio/snapshot.
 */
export interface AudioSnapshot {
  controller: HifiController;
  zones: HifiZone[];
  sources: HifiSource[];
  groups: HifiGroup[];
  shairportZoneSettings: ShairportZoneSettings[];
  pcm: PcmState;
  services: AudioServiceStatus;
  hifiConnected: boolean;
}

/**
 * API envelope for audio endpoints.
 */
export interface AudioApiResponse<T> {
  ok: boolean;
  data?: T;
  error?: { code: string; message: string };
}

/**
 * Zone settings save payload.
 */
export interface ZoneSettingsPatch {
  controller?: Partial<{
    name: string;
    enabled: number;
    power: number;
    volume: number;
    treble: number;
    bass: number;
    balance: number;
    loudness: number;
    initialVolume: number;
    pageVolume: number;
    groupNumber: number;
  }>;
  shairport?: Partial<{
    volumeControlProfile: string;
    activeStateTimeout: number;
    sessionTimeout: number;
    logVerbosity: number;
  }>;
}
