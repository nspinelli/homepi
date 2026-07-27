import { createContext, useContext, type ReactNode } from "react";

import { useSensorsModuleState } from "@/hooks/use-sensors-module.js";

/** Shared sensors module context value. */
export type SensorsModuleContextValue = ReturnType<typeof useSensorsModuleState>;

const SensorsModuleContext = createContext<SensorsModuleContextValue | null>(null);

/**
 * Provides a single contact sensors state tree for the whole app.
 * @param props - Provider props.
 * @param props.children - Child tree.
 * @returns Provider element.
 */
export function SensorsModuleProvider({ children }: { children: ReactNode }): React.JSX.Element {
  const value = useSensorsModuleState();
  return <SensorsModuleContext.Provider value={value}>{children}</SensorsModuleContext.Provider>;
}

/**
 * Reads shared contact sensors state and actions from context.
 * @returns Sensors module state and actions.
 */
export function useSensorsModule(): SensorsModuleContextValue {
  const context = useContext(SensorsModuleContext);
  if (!context) {
    throw new Error("useSensorsModule must be used within SensorsModuleProvider");
  }
  return context;
}
