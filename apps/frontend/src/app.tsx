import { StatusDashboard } from "./components/status-dashboard.js";
import { useSystemDashboard } from "./hooks/use-system-dashboard.js";

/**
 * HomePi frontend application shell with the system status dashboard.
 */
export function App(): React.JSX.Element {
  const { state, refresh } = useSystemDashboard();

  return (
    <div className="app">
      <StatusDashboard
        state={state}
        onRefresh={() => {
          void refresh();
        }}
      />
    </div>
  );
}
