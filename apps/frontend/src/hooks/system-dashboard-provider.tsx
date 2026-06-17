import { createContext, useContext, type ReactNode } from "react";

import { useSystemDashboardState } from "@/hooks/use-system-dashboard.js";

/** Shared system dashboard context value. */
export type SystemDashboardContextValue = ReturnType<typeof useSystemDashboardState>;

const SystemDashboardContext = createContext<SystemDashboardContextValue | null>(null);

/**
 * Provides a single system dashboard state tree for the whole app.
 * @param props - Provider props.
 * @param props.children - Child tree.
 * @returns Provider element.
 */
export function SystemDashboardProvider({ children }: { children: ReactNode }): React.JSX.Element {
  const value = useSystemDashboardState();
  return (
    <SystemDashboardContext.Provider value={value}>{children}</SystemDashboardContext.Provider>
  );
}

/**
 * Reads shared system dashboard state from context.
 * @returns Dashboard state and refresh handler.
 */
export function useSystemDashboard(): SystemDashboardContextValue {
  const context = useContext(SystemDashboardContext);
  if (!context) {
    throw new Error("useSystemDashboard must be used within SystemDashboardProvider");
  }
  return context;
}
