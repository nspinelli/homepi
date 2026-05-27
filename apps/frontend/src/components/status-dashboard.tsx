import type { SystemDashboardState } from "../hooks/use-system-dashboard.js";
import type { SystemStatusSnapshot } from "../types/dashboard-types.js";

/**
 * Props for the system status dashboard.
 */
export interface StatusDashboardProps {
  /** Dashboard state from useSystemDashboard. */
  state: SystemDashboardState;
  /** Manual refresh handler. */
  onRefresh: () => void;
}

/**
 * Renders the HomePi system status dashboard.
 * @param props - Dashboard props.
 * @returns Dashboard JSX.
 */
export function StatusDashboard({ state, onRefresh }: StatusDashboardProps): React.JSX.Element {
  const status = state.systemStatus;

  return (
    <div className="dashboard">
      <header className="dashboard-header">
        <div>
          <p className="eyebrow">HomePi</p>
          <h1>System Status</h1>
        </div>
        <button type="button" className="refresh-button" onClick={onRefresh}>
          Refresh
        </button>
      </header>

      {state.error ? <p className="banner error">{state.error}</p> : null}
      {state.loading ? <p className="banner">Loading platform status…</p> : null}

      <section className="grid two-up">
        <StatusCard title="Backend health" value={state.health?.status ?? "unknown"} />
        <StatusCard
          title="API correlation"
          value={state.health ? "present" : "pending"}
          detail={state.lastEvent?.correlationId}
        />
      </section>

      <section className="grid connections">
        <ConnectionCard label="SSE /events" state={state.sseState} />
        <ConnectionCard label="WebSocket /ws" state={state.wsState} />
      </section>

      <section className="card metrics">
        <h2>Platform</h2>
        <div className="metric-grid">
          <Metric label="Uptime" value={formatUptime(status?.uptimeMs)} />
          <Metric
            label="Last event"
            value={status?.lastEventAt ? formatTimestamp(status.lastEventAt) : "—"}
          />
        </div>
      </section>

      <section className="card services">
        <h2>Core services</h2>
        <ul className="service-list">
          {renderServiceRow("backend", status?.backend)}
          {renderServiceRow("config", status?.config)}
          {renderServiceRow("logging", status?.logging)}
          {renderServiceRow("runtime", status?.runtime)}
          {renderServiceRow("transport", status?.transport)}
          {renderServiceRow("events", status?.events)}
          {renderServiceRow("state", status?.state)}
          {renderServiceRow("api", status?.api)}
        </ul>
      </section>

      <section className="card last-event">
        <h2>Last received event</h2>
        {state.lastEvent ? (
          <pre>{JSON.stringify(state.lastEvent, null, 2)}</pre>
        ) : (
          <p className="muted">Waiting for SSE events…</p>
        )}
      </section>
    </div>
  );
}

/**
 * Renders a labeled status card.
 */
function StatusCard({
  title,
  value,
  detail,
}: {
  title: string;
  value: string;
  detail?: string;
}): React.JSX.Element {
  return (
    <article className="card status-card">
      <h2>{title}</h2>
      <p className={`status-pill ${value}`}>{value}</p>
      {detail ? <p className="muted detail">{detail}</p> : null}
    </article>
  );
}

/**
 * Renders a live transport connection card.
 */
function ConnectionCard({
  label,
  state,
}: {
  label: string;
  state: string;
}): React.JSX.Element {
  return (
    <article className="card connection-card">
      <h2>{label}</h2>
      <p className={`status-pill ${state}`}>{state}</p>
    </article>
  );
}

/**
 * Renders a compact metric row.
 */
function Metric({ label, value }: { label: string; value: string }): React.JSX.Element {
  return (
    <div className="metric">
      <span className="metric-label">{label}</span>
      <span className="metric-value">{value}</span>
    </div>
  );
}

/**
 * Renders one core service status row.
 */
function renderServiceRow(
  name: string,
  value: SystemStatusSnapshot[keyof SystemStatusSnapshot] | undefined
): React.JSX.Element {
  return (
    <li key={name}>
      <span>{name}</span>
      <span className={`status-pill ${String(value ?? "unknown")}`}>{String(value ?? "—")}</span>
    </li>
  );
}

/**
 * Formats uptime milliseconds for display.
 * @param uptimeMs - Uptime in milliseconds.
 * @returns Human-readable uptime string.
 */
function formatUptime(uptimeMs: number | undefined): string {
  if (uptimeMs === undefined) {
    return "—";
  }
  const totalSeconds = Math.floor(uptimeMs / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  return `${hours}h ${minutes}m ${seconds}s`;
}

/**
 * Formats an ISO timestamp for display.
 * @param value - ISO8601 timestamp.
 * @returns Locale formatted timestamp.
 */
function formatTimestamp(value: string): string {
  return new Date(value).toLocaleString();
}
