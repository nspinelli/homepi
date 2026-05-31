import type { Logger } from "@homepi/core-logging";

import type { SystemStatusSnapshot } from "../types/system-status-types.js";
import {
  journalServiceToStatusField,
  mapLifecycleEventToStatus,
} from "./service-status-mappers.js";
import type { StatusUpdateCoordinator } from "./status-update-coordinator.js";

const LIFECYCLE_MODULE = "core.runtime";

/**
 * Options for journald-derived service status updates.
 */
export interface JournalServiceStatusBridgeOptions {
  /** Structured logger. */
  logger: Logger;
  /** Status update coordinator. */
  coordinator: StatusUpdateCoordinator;
}

/**
 * Maps structured journald lifecycle logs to dashboard service status fields.
 */
export class JournalServiceStatusBridge {
  /**
   * @param options - Bridge configuration.
   */
  constructor(private readonly options: JournalServiceStatusBridgeOptions) {}

  /**
   * Handles a parsed journal JSON log line for lifecycle status updates.
   * @param log - Parsed journal log fields.
   */
  handleLogLine(log: {
    ts: string;
    service: string;
    module: string;
    event: string;
  }): void {
    if (log.module !== LIFECYCLE_MODULE) {
      return;
    }

    const field = journalServiceToStatusField(log.service);
    if (!field) {
      return;
    }

    const status = mapLifecycleEventToStatus(log.event);
    if (!status) {
      return;
    }

    const patch: Partial<SystemStatusSnapshot> = { [field]: status };
    this.options.coordinator.patchAndBroadcast(patch, "journal-lifecycle", log.ts);
  }
}
