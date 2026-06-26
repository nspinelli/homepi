/**
 * Generic Unix-socket response shape used by paging services.
 */
export interface PagingSocketResponse<T> {
  /** True when the service accepted and handled the request. */
  ok: boolean;
  /** Response payload for successful requests. */
  data?: T;
  /** Optional error object for failed requests. */
  error?: {
    /** Stable machine-readable error code. */
    code?: string;
    /** Human-readable failure message. */
    message?: string;
  };
}

/**
 * API key metadata stored by the paging service.
 */
export interface PagingApiKeyMetadata {
  /** True when a valid key hash is configured. */
  configured: boolean;
  /** Public key prefix shown in the UI. */
  prefix: string | null;
  /** Scrypt-formatted hash stored server-side. */
  hash?: string | null;
}

/**
 * Runtime dependencies reported by the paging service.
 */
export interface PagingStatusDependencies {
  /** True when paging is enabled. */
  enabled: boolean;
  /** True when a paging DAC is configured. */
  pagingDacConfigured: boolean;
  /** True when the configured DAC is currently connected. */
  pagingDacConnected: boolean;
  /** True when the DAC output is currently open. */
  dacOpen: boolean;
  /** True when the default voice is installed. */
  defaultVoiceInstalled: boolean;
  /** True when the TTS worker is ready. */
  ttsWorkerReady: boolean;
  /** True when the HiFi serial service is connected. */
  hifiConnected: boolean;
}

/**
 * Live paging service status returned by the backend API.
 */
export interface PagingStatus {
  /** High-level operational state label. */
  state: string;
  /** True when paging can accept jobs. */
  ready: boolean;
  /** Resource state: DISABLED, COLD, WARM, or ACTIVE. */
  resourceState: string;
  /** True when a paging job is currently active. */
  busy: boolean;
  /** Active job identifier when a job is running. */
  currentJobId: string | null;
  /** Dependency breakdown used by readiness checks. */
  dependencies: PagingStatusDependencies;
}

/**
 * Persistent paging configuration.
 */
export interface PagingConfig {
  /** True when paging automation is enabled. */
  enabled: boolean;
  /** Active paging DAC device identifier. */
  pagingDacDeviceId: string | null;
  /** Source path used for DAC assignment resolution. */
  pagingDacSource: string | null;
  /** Default voice identifier. */
  defaultVoiceId: string | null;
  /** Active voice identifier. */
  activeVoiceId: string | null;
  /** Active chime identifier. */
  activeChimeId: string | null;
  /** Maximum number of installed voices. */
  maxInstalledVoices: number;
  /** Maximum text length for speak jobs. */
  maxTextLength: number;
  /** Maximum text length for preview jobs. */
  maxPreviewTextLength: number;
  /** Character threshold for streaming synthesis mode. */
  streamThresholdChars: number;
  /** Default busy behavior for automation callers. */
  defaultOnBusy: string;
  /** Idle policy label for warm/cold behavior. */
  idlePolicy: string;
  /** Idle timeout in milliseconds for warm_with_timeout policy. */
  idleWarmTimeoutMs: number;
  /** DAC close delay after jobs complete, in milliseconds. */
  dacIdleCloseDelayMs: number;
  /** Whether to keep debug audio artifacts. */
  keepLastAudioForDebug: boolean;
  /** Embedded status payload mirrored from the paging service. */
  status?: PagingStatus;
  /** API key metadata attached by the paging service. */
  apiKey?: PagingApiKeyMetadata;
}

/**
 * Mutable paging configuration fields accepted by PUT config.
 */
export type PagingConfigUpdate = Partial<
  Pick<
    PagingConfig,
    | "enabled"
    | "defaultVoiceId"
    | "activeVoiceId"
    | "activeChimeId"
    | "idlePolicy"
    | "idleWarmTimeoutMs"
    | "dacIdleCloseDelayMs"
    | "defaultOnBusy"
  >
> &
  Record<string, unknown>;

/**
 * Voice metadata from the paging catalog and local install state.
 */
export interface PagingVoice {
  /** Stable voice identifier. */
  voiceId: string;
  /** Friendly display name. */
  displayName: string;
  /** BCP-47 style language code. */
  languageCode: string;
  /** Voice quality tier. */
  quality: string;
  /** True when this voice is installed locally. */
  installed: boolean;
  /** True when this voice is the configured default. */
  isDefault: boolean;
  /** True when this voice ships with HomePi. */
  isBundled: boolean;
  /** True when a sample is available for browser playback. */
  sampleAvailable: boolean;
  /** Optional sample URL for remote preview. */
  sampleUrl?: string;
}

/**
 * Chime metadata returned by paging endpoints.
 */
export interface PagingChime {
  /** Stable chime identifier. */
  chimeId: string;
  /** Friendly display name. */
  displayName: string;
  /** True when this chime is currently active. */
  isActive: boolean;
  /** True when this chime is bundled and protected. */
  isBundled: boolean;
}

/**
 * Speak request payload accepted by paging command routes.
 */
export interface PagingSpeakRequest {
  /** Text to synthesize and page. */
  text: string;
  /** Optional voice override. */
  voiceId?: string | null;
  /** Logical source label for auditing. */
  source?: string;
  /** True to prepend a chime before speech. */
  includeChime?: boolean;
  /** Optional chime override when includeChime is true. */
  chimeId?: string | null;
  /** Busy behavior for queuing/rejection. */
  onBusy?: string;
  /** Completion mode for HTTP responses. */
  waitUntil?: string;
}

/**
 * Chime-only request payload accepted by paging command routes.
 */
export interface PagingChimeRequest {
  /** Optional chime override. */
  chimeId?: string | null;
  /** Logical source label for auditing. */
  source?: string;
  /** Busy behavior for queuing/rejection. */
  onBusy?: string;
  /** Completion mode for HTTP responses. */
  waitUntil?: string;
}

/**
 * Voice preview request payload.
 */
export interface PagingVoicePreviewRequest {
  /** Installed voice identifier to preview. */
  voiceId: string;
  /** Preview text to synthesize. */
  text: string;
  /** Output target selector for the preview flow. */
  output?: string;
}

/**
 * Whole-house preview request payload.
 */
export interface PagingPagePreviewRequest {
  /** Optional installed voice identifier override. */
  voiceId?: string | null;
  /** Preview text to synthesize for the whole house. */
  text: string;
  /** True to prepend a chime. */
  includeChime?: boolean;
  /** Optional chime override when includeChime is true. */
  chimeId?: string | null;
}
