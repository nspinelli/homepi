import type { EventEnvelope } from "../types/dashboard-types.js";

/** Event names excluded from the status page activity log. */
const EXCLUDED_EVENTS = new Set([
  "heartbeat",
  "system_status_snapshot",
  "system_status_delta",
  "audio.realtime",
  "metadata_progress_updated",
]);

/**
 * Returns true when an event should appear in the status activity log.
 * @param envelope - SSE or broker event envelope.
 * @returns Whether the event is user-visible.
 */
export function isUiVisibleEvent(envelope: EventEnvelope | { event?: string; payload?: Record<string, unknown> }): boolean {
  if (envelope.payload?.uiVisible === false) {
    return false;
  }

  const eventName = envelope.event ?? "";
  if (EXCLUDED_EVENTS.has(eventName)) {
    return false;
  }

  if (eventName === "log_record") {
    const level = String(envelope.payload?.level ?? "INFO").toUpperCase();
    if (level === "DEBUG" || level === "TRACE") {
      return false;
    }
  }

  return true;
}
