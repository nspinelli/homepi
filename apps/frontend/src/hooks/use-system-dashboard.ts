import { useCallback, useEffect, useRef, useState } from "react";
import { getAppConfig } from "../config/app-config.js";
import type {
  ApiResponse,
  ConnectionState,
  CoreStatusPayload,
  DashboardLoadState,
  EventEnvelope,
  HealthReport,
  HostMetricsSnapshot,
} from "../types/dashboard-types.js";
import { isUiVisibleEvent } from "../lib/activity-log-filter.js";

/**
 * Dashboard data and live connection state for the system status slice.
 */
export interface SystemDashboardState {
  health: HealthReport | null;
  coreStatus: CoreStatusPayload | null;
  hostMetrics: HostMetricsSnapshot | null;
  lastEvent: EventEnvelope | null;
  recentEvents: EventEnvelope[];
  sseState: ConnectionState;
  wsState: ConnectionState;
  error: string | null;
  transportError: string | null;
  loadState: DashboardLoadState;
  loading: boolean;
  lastFetchedAt: string | null;
}

const MAX_RECENT_EVENTS = 200;
const WS_RECONNECT_MS = 3_000;

const initialState: SystemDashboardState = {
  health: null,
  coreStatus: null,
  hostMetrics: null,
  lastEvent: null,
  recentEvents: [],
  sseState: "disconnected",
  wsState: "disconnected",
  error: null,
  transportError: null,
  loadState: "loading",
  loading: true,
  lastFetchedAt: null,
};

/**
 * Extracts host metrics from SSE/WS status payloads.
 * @param payload - Status snapshot or delta payload.
 * @returns Host metrics when present.
 */
function extractHostMetrics(payload: Record<string, unknown> | undefined): HostMetricsSnapshot | null {
  if (!payload) {
    return null;
  }

  const snapshot = (payload.snapshot ?? payload.status) as HostMetricsSnapshot | undefined;
  if (
    snapshot &&
    typeof snapshot.uptimeMs === "number" &&
    "cpuTempC" in snapshot &&
    "lastEventAt" in snapshot
  ) {
    return snapshot;
  }

  return null;
}

/**
 * Prepends an event to the rolling recent-events buffer when UI-visible.
 * @param events - Current recent events.
 * @param envelope - New event envelope.
 * @returns Updated recent events list.
 */
function prependRecentEvent(events: EventEnvelope[], envelope: EventEnvelope): EventEnvelope[] {
  if (!isUiVisibleEvent(envelope)) {
    return events;
  }
  return [envelope, ...events.filter((item) => item.id !== envelope.id)].slice(0, MAX_RECENT_EVENTS);
}

/**
 * Extracts a human-readable error from an API response.
 * @param response - Fetch response.
 * @param json - Parsed API envelope.
 * @returns Error message.
 */
function extractApiError(response: Response, json: ApiResponse<unknown>): string {
  if (json.error?.message) {
    return json.error.message;
  }
  return `Request failed (${response.status} ${response.statusText})`;
}

/**
 * Loads REST status and maintains SSE/WebSocket live connections.
 * @returns Dashboard state and refresh handler.
 */
export function useSystemDashboardState(): {
  state: SystemDashboardState;
  refresh: () => Promise<void>;
} {
  const [state, setState] = useState<SystemDashboardState>(initialState);
  const eventSourceRef = useRef<EventSource | null>(null);
  const webSocketRef = useRef<WebSocket | null>(null);
  const pingTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const wsGenerationRef = useRef(0);
  const wsReconnectTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const restSucceededRef = useRef(false);

  const applyEvent = useCallback((envelope: EventEnvelope) => {
    const hostMetrics =
      envelope.event === "system_status_snapshot" || envelope.event === "system_status_delta"
        ? extractHostMetrics(envelope.payload)
        : null;

    setState((current) => ({
      ...current,
      lastEvent: envelope,
      recentEvents: prependRecentEvent(current.recentEvents, envelope),
      hostMetrics: hostMetrics ?? current.hostMetrics,
    }));
  }, []);

  const fetchRest = useCallback(async () => {
    const config = getAppConfig();
    setState((current) => ({ ...current, loading: true, error: null, loadState: "loading" }));

    try {
      const [healthRes, coreRes] = await Promise.all([
        fetch(`${config.apiBaseUrl}/api/health`),
        fetch(`${config.apiBaseUrl}/api/core/status`),
      ]);

      const healthJson = (await healthRes.json()) as ApiResponse<HealthReport>;
      const coreJson = (await coreRes.json()) as ApiResponse<CoreStatusPayload>;

      if (!healthRes.ok) {
        throw new Error(extractApiError(healthRes, healthJson));
      }
      if (!coreRes.ok) {
        throw new Error(extractApiError(coreRes, coreJson));
      }
      if (!healthJson.ok || !coreJson.ok) {
        throw new Error(
          healthJson.error?.message ??
            coreJson.error?.message ??
            "One or more status endpoints returned an error envelope"
        );
      }

      restSucceededRef.current = true;

      setState((current) => ({
        ...current,
        health: healthJson.data ?? null,
        coreStatus: coreJson.data ?? null,
        hostMetrics: coreJson.data?.host ?? current.hostMetrics,
        loading: false,
        loadState: coreJson.data?.healthServiceReachable === false ? "stale" : "ready",
        error: coreJson.data?.healthServiceReachable === false
          ? "Health monitoring is unavailable. Showing last known data where possible."
          : null,
        lastFetchedAt: new Date().toISOString(),
      }));
    } catch (error) {
      const message = error instanceof Error ? error.message : "Failed to load status";
      setState((current) => ({
        ...current,
        loading: false,
        loadState: "error",
        error: message,
      }));
    }
  }, []);

  const connectSse = useCallback(() => {
    const config = getAppConfig();
    setState((current) => ({ ...current, sseState: "connecting" }));

    const source = new EventSource(config.eventsUrl);
    eventSourceRef.current = source;

    source.onopen = () => {
      setState((current) => ({
        ...current,
        sseState: "connected",
        transportError:
          current.transportError?.includes("SSE") === true ? null : current.transportError,
      }));
    };

    source.onerror = () => {
      setState((current) => ({
        ...current,
        sseState: "error",
        transportError: "Live event stream (SSE) is disconnected.",
      }));
    };

    const handleEnvelope = (event: MessageEvent<string>) => {
      try {
        const envelope = JSON.parse(event.data) as EventEnvelope;
        applyEvent(envelope);
      } catch {
        setState((current) => ({
          ...current,
          error: "Failed to parse SSE event envelope",
          loadState: restSucceededRef.current ? "stale" : "error",
        }));
      }
    };

    source.addEventListener("envelope", handleEnvelope);
  }, [applyEvent]);

  const connectWebSocket = useCallback(() => {
    const config = getAppConfig();
    const generation = ++wsGenerationRef.current;
    setState((current) => ({ ...current, wsState: "connecting" }));

    const socket = new WebSocket(config.wsUrl);
    webSocketRef.current = socket;

    socket.onopen = () => {
      if (generation !== wsGenerationRef.current) {
        return;
      }
      setState((current) => ({
        ...current,
        wsState: "connected",
        transportError:
          current.transportError?.includes("WebSocket") === true ? null : current.transportError,
      }));

      if (pingTimerRef.current) {
        clearInterval(pingTimerRef.current);
      }
      pingTimerRef.current = setInterval(() => {
        if (socket.readyState === WebSocket.OPEN) {
          socket.send(JSON.stringify({ type: "ping" }));
        }
      }, 30_000);
    };

    socket.onerror = () => {
      if (generation !== wsGenerationRef.current) {
        return;
      }
      setState((current) => ({
        ...current,
        wsState: "error",
        transportError: "Live status WebSocket is disconnected.",
      }));
    };

    socket.onclose = () => {
      if (generation !== wsGenerationRef.current) {
        return;
      }
      if (pingTimerRef.current) {
        clearInterval(pingTimerRef.current);
        pingTimerRef.current = null;
      }
      setState((current) => ({
        ...current,
        wsState: "disconnected",
        transportError: "Live status WebSocket is disconnected.",
      }));
      if (wsReconnectTimerRef.current) {
        clearTimeout(wsReconnectTimerRef.current);
      }
      wsReconnectTimerRef.current = setTimeout(() => {
        if (generation === wsGenerationRef.current) {
          connectWebSocket();
        }
      }, WS_RECONNECT_MS);
    };

    socket.onmessage = (message) => {
      try {
        const envelope = JSON.parse(String(message.data)) as {
          type?: string;
          payload?: Record<string, unknown>;
        };

        const hostMetrics = extractHostMetrics(envelope.payload);
        if (hostMetrics) {
          setState((current) => ({
            ...current,
            hostMetrics,
          }));
        }
      } catch {
        setState((current) => ({
          ...current,
          error: "Failed to parse WebSocket message",
          loadState: restSucceededRef.current ? "stale" : "error",
        }));
      }
    };
  }, []);

  useEffect(() => {
    void fetchRest();
    connectSse();
    connectWebSocket();

    return () => {
      wsGenerationRef.current += 1;
      eventSourceRef.current?.close();
      webSocketRef.current?.close();
      if (pingTimerRef.current) {
        clearInterval(pingTimerRef.current);
        pingTimerRef.current = null;
      }
      if (wsReconnectTimerRef.current) {
        clearTimeout(wsReconnectTimerRef.current);
        wsReconnectTimerRef.current = null;
      }
    };
  }, [connectSse, connectWebSocket, fetchRest]);

  return {
    state,
    refresh: fetchRest,
  };
}
