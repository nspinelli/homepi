/**
 * Frontend runtime configuration sourced from Vite environment variables.
 */
export interface AppConfig {
  /** Base URL for REST API calls (empty uses same origin). */
  apiBaseUrl: string;
  /** SSE events endpoint URL. */
  eventsUrl: string;
  /** WebSocket endpoint URL. */
  wsUrl: string;
}

/**
 * Resolves frontend configuration from Vite `import.meta.env`.
 * @returns Application configuration.
 */
export function getAppConfig(): AppConfig {
  const apiBaseUrl = import.meta.env.VITE_API_BASE_URL ?? "";
  const origin =
    typeof window !== "undefined" && window.location.origin
      ? window.location.origin
      : "http://127.0.0.1:5173";

  const base = apiBaseUrl || origin;

  return {
    apiBaseUrl: base,
    eventsUrl: import.meta.env.VITE_EVENTS_URL ?? `${base}/events`,
    wsUrl: import.meta.env.VITE_WS_URL ?? `${toWebSocketUrl(base)}/ws`,
  };
}

/**
 * Converts an HTTP(S) origin to a WebSocket origin.
 * @param url - HTTP origin or base URL.
 * @returns WebSocket origin.
 */
function toWebSocketUrl(url: string): string {
  if (url.startsWith("https://")) {
    return `wss://${url.slice("https://".length)}`;
  }
  if (url.startsWith("http://")) {
    return `ws://${url.slice("http://".length)}`;
  }
  return url;
}
