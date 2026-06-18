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
  firmwareVersion?: string;
  hardwareVersion?: string;
  deviceName?: string;
  macAddress?: string;
  dhcpEnabled?: number;
  ipAddress?: string;
  subnetMask?: string;
  gateway?: string;
  tcpPort?: number;
  pageActive?: number;
  serialDeviceId?: string;
  serialPath?: string;
  lastFullSyncAt?: string;
  updatedAt?: string;
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
  clientModel?: string;
}

/**
 * PCM router playback progress and transport state for the DAC owner zone.
 */
export interface PcmPlayback {
  playing: boolean;
  positionMs: number;
  durationMs: number;
  /** Wall-clock ms when positionMs was last synced from the AirPlay source. */
  progressSyncedAt?: number;
}

/**
 * PCM profile tuple from pcm_router_snapshot v2.
 */
export interface PcmProfileTuple {
  sampleRate: number;
  channels: number;
  sampleFormat: string;
}

/**
 * PCM router live routing state.
 */
export interface PcmState {
  ownerZoneId: number;
  activeStack: number[];
  dacState: string;
  profileMode?: string;
  profileStatus?: string;
  loopbackProfile?: PcmProfileTuple;
  dacProfile?: PcmProfileTuple;
  profileRevision?: number;
  profileSource?: string;
  audioBridgeState?: string;
  metadata: PcmMetadata;
  playback: PcmPlayback;
  /** True when pcm-router has cached cover art for the owner zone. */
  hasCoverArt?: boolean;
}

/**
 * Shairport remote commands accepted by the playback API.
 */
export type PlaybackRemoteCommand =
  | "play"
  | "pause"
  | "playpause"
  | "playresume"
  | "stop"
  | "nextitem"
  | "previtem"
  | "volumedown"
  | "volumeup"
  | "mutetoggle"
  | "shuffle_songs";

/**
 * View model for the Home Audio player bar.
 */
export interface AudioPlaybackView {
  visible: boolean;
  zoneId: number;
  zoneName: string;
  track?: string;
  artist?: string;
  album?: string;
  clientName?: string;
  sourceLabel?: string;
  playing: boolean;
  positionMs: number;
  durationMs: number;
  /** Wall-clock ms when positionMs was last synced from the source. */
  progressSyncedAt: number;
  /** Hi-Fi zone volume 0–100 (synced with AirPlay via the shairport hook). */
  volume: number;
  coverUrl?: string;
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
 * Controller settings save payload.
 */
export interface ControllerSettingsPatch {
  deviceName: string;
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

/**
 * Source settings save payload.
 */
export interface SourceSettingsPatch {
  controller?: Partial<{
    name: string;
    enabled: number;
    inputGain: number;
    displayLine: string;
  }>;
  /** When true, designates this source as the exclusive AirPlay slot. */
  airplay?: boolean;
}
