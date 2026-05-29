import { useCallback, useEffect, useRef, useState } from "react";
import { getAppConfig } from "../config/app-config.js";
import type {
  ApiResponse,
  ConnectionState,
  CoreStatusPayload,
  EventEnvelope,
  HealthReport,
  SystemStatusSnapshot,
} from "../types/dashboard-types.js";

/**
 * Dashboard data and live connection state for the system status slice.
 */
export interface SystemDashboardState {
  health: HealthReport | null;
  coreStatus: CoreStatusPayload | null;
  systemStatus: SystemStatusSnapshot | null;
  lastEvent: EventEnvelope | null;
  recentEvents: EventEnvelope[];
  sseState: ConnectionState;
  wsState: ConnectionState;
  error: string | null;
  loading: boolean;
}

const MAX_RECENT_EVENTS = 50;

const initialState: SystemDashboardState = {
  health: null,
  coreStatus: null,
  systemStatus: null,
  lastEvent: null,
  recentEvents: [],
  sseState: "disconnected",
  wsState: "disconnected",
  error: null,
  loading: true,
};

/**
 * Prepends an event to the rolling recent-events buffer.
 * @param events - Current recent events.
 * @param envelope - New event envelope.
 * @returns Updated recent events list.
 */
function prependRecentEvent(events: EventEnvelope[], envelope: EventEnvelope): EventEnvelope[] {
  return [envelope, ...events.filter((item) => item.id !== envelope.id)].slice(0, MAX_RECENT_EVENTS);
}

/**
 * Loads REST status and maintains SSE/WebSocket live connections.
 * @returns Dashboard state and refresh handler.
 */
export function useSystemDashboard(): {
  state: SystemDashboardState;
  refresh: () => Promise<void>;
} {
  const [state, setState] = useState<SystemDashboardState>(initialState);
  const eventSourceRef = useRef<EventSource | null>(null);
  const webSocketRef = useRef<WebSocket | null>(null);
  const pingTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);

  const applyEvent = useCallback((envelope: EventEnvelope) => {
    const snapshot =
      envelope.event === "system_status_snapshot"
        ? (envelope.payload.snapshot as SystemStatusSnapshot | undefined)
        : envelope.event === "system_status_delta"
          ? (envelope.payload.status as SystemStatusSnapshot | undefined)
          : undefined;

    setState((current) => ({
      ...current,
      lastEvent: envelope,
      recentEvents: prependRecentEvent(current.recentEvents, envelope),
      systemStatus: snapshot ?? current.systemStatus,
    }));
  }, []);

  const fetchRest = useCallback(async () => {
    const config = getAppConfig();
    setState((current) => ({ ...current, loading: true, error: null }));

    try {
      const [healthRes, coreRes] = await Promise.all([
        fetch(`${config.apiBaseUrl}/api/health`),
        fetch(`${config.apiBaseUrl}/api/core/status`),
      ]);

      const healthJson = (await healthRes.json()) as ApiResponse<HealthReport>;
      const coreJson = (await coreRes.json()) as ApiResponse<CoreStatusPayload>;

      if (!healthJson.ok || !coreJson.ok) {
        throw new Error("One or more status endpoints returned an error envelope");
      }

      setState((current) => ({
        ...current,
        health: healthJson.data ?? null,
        coreStatus: coreJson.data ?? null,
        systemStatus: coreJson.data?.system ?? current.systemStatus,
        loading: false,
      }));
    } catch (error) {
      const message = error instanceof Error ? error.message : "Failed to load status";
      setState((current) => ({
        ...current,
        loading: false,
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
      setState((current) => ({ ...current, sseState: "connected", error: null }));
    };

    source.onerror = () => {
      setState((current) => ({ ...current, sseState: "error" }));
    };

    const handleEnvelope = (event: MessageEvent<string>) => {
      try {
        const envelope = JSON.parse(event.data) as EventEnvelope;
        applyEvent(envelope);
      } catch {
        setState((current) => ({
          ...current,
          error: "Failed to parse SSE event envelope",
        }));
      }
    };

    source.addEventListener("system_status_snapshot", handleEnvelope);
    source.addEventListener("system_status_delta", handleEnvelope);
    source.addEventListener("heartbeat", handleEnvelope);
  }, [applyEvent]);

  const connectWebSocket = useCallback(() => {
    const config = getAppConfig();
    setState((current) => ({ ...current, wsState: "connecting" }));

    const socket = new WebSocket(config.wsUrl);
    webSocketRef.current = socket;

    socket.onopen = () => {
      setState((current) => ({ ...current, wsState: "connected", error: null }));

      pingTimerRef.current = setInterval(() => {
        if (socket.readyState === WebSocket.OPEN) {
          socket.send(JSON.stringify({ type: "ping" }));
        }
      }, 30_000);
    };

    socket.onerror = () => {
      setState((current) => ({ ...current, wsState: "error" }));
    };

    socket.onclose = () => {
      setState((current) => ({ ...current, wsState: "disconnected" }));
    };

    socket.onmessage = (message) => {
      try {
        const envelope = JSON.parse(String(message.data)) as {
          type?: string;
          payload?: { snapshot?: SystemStatusSnapshot; action?: string };
        };

        if (envelope.type === "snapshot" && envelope.payload?.snapshot) {
          setState((current) => ({
            ...current,
            systemStatus: envelope.payload?.snapshot ?? current.systemStatus,
          }));
        }
      } catch {
        setState((current) => ({
          ...current,
          error: "Failed to parse WebSocket message",
        }));
      }
    };
  }, []);

  useEffect(() => {
    void fetchRest();
    connectSse();
    connectWebSocket();

    return () => {
      eventSourceRef.current?.close();
      webSocketRef.current?.close();
      if (pingTimerRef.current) {
        clearInterval(pingTimerRef.current);
      }
    };
  }, [connectSse, connectWebSocket, fetchRest]);

  return {
    state,
    refresh: fetchRest,
  };
}
