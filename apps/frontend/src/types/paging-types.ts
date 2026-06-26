/**
 * Paging resource lifecycle states exposed by backend status/config.
 */
export type PagingResourceState = "DISABLED" | "COLD" | "WARM" | "ACTIVE";

/**
 * Idle policy options for the paging worker.
 */
export type PagingIdlePolicy = "always_warm" | "warm_with_timeout";

/**
 * Combined readiness details surfaced with paging config.
 */
export interface PagingReadinessStatus {
  ready: boolean;
  resourceState: PagingResourceState;
  dacConnected: boolean;
  dacOpen: boolean;
  voiceLoaded: boolean;
  hifiConnected: boolean;
  busy: boolean;
}

/**
 * Persisted paging configuration from `/api/audio/paging/config`.
 */
export interface PagingConfig {
  enabled: boolean;
  pagingDacDeviceId: string | null;
  pagingDacSource?: string;
  defaultVoiceId: string | null;
  activeVoiceId: string | null;
  activeChimeId: string | null;
  maxInstalledVoices: number;
  maxTextLength: number;
  maxPreviewTextLength: number;
  streamThresholdChars: number;
  defaultOnBusy: "reject";
  idlePolicy: PagingIdlePolicy;
  idleWarmTimeoutMs: number;
  dacIdleCloseDelayMs: number;
  keepLastAudioForDebug?: boolean;
  status?: PagingReadinessStatus;
}

/**
 * Dependency detail block from `/api/audio/paging/status`.
 */
export interface PagingStatusDependencies {
  enabled: boolean;
  pagingDacConfigured: boolean;
  pagingDacConnected: boolean;
  dacOpen: boolean;
  defaultVoiceInstalled: boolean;
  ttsWorkerReady: boolean;
  hifiConnected: boolean;
}

/**
 * Live paging status and current job metadata.
 */
export interface PagingStatus {
  state: string;
  ready: boolean;
  resourceState: PagingResourceState;
  busy: boolean;
  currentJobId: string | null;
  dependencies: PagingStatusDependencies;
}

/**
 * Voice catalog row used by paging voice browser.
 */
export interface PagingVoice {
  voiceId: string;
  displayName: string;
  languageCode: string;
  accent?: string;
  quality: string;
  installed: boolean;
  isDefault: boolean;
  isBundled: boolean;
  sampleAvailable: boolean;
  sampleUrl?: string | null;
}

/**
 * Chime row used by paging chime manager UI.
 */
export interface PagingChime {
  chimeId: string;
  displayName: string;
  isActive: boolean;
  isBundled: boolean;
  durationMs?: number;
  sizeBytes?: number;
}

/**
 * Paging API key summary from `/api/audio/settings`.
 */
export interface PagingApiKeyInfo {
  pagingApiKeyConfigured: boolean;
  pagingApiKeyPrefix: string | null;
}

/**
 * Response from API key save endpoint.
 */
export interface PagingApiKeySetResult {
  ok: boolean;
  pagingApiKeyConfigured: boolean;
  pagingApiKeyPrefix: string | null;
}

/**
 * Partial config update payload for paging settings.
 */
export interface PagingConfigPatch {
  enabled?: boolean;
  defaultVoiceId?: string;
  activeChimeId?: string;
  idlePolicy?: PagingIdlePolicy;
  idleWarmTimeoutMs?: number;
  dacIdleCloseDelayMs?: number;
}
