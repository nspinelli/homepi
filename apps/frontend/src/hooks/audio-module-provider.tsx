import { createContext, useContext, type ReactNode } from "react";

import { useAudioModuleState } from "@/hooks/use-audio-module.js";

/** Shared audio module context value. */
export type AudioModuleContextValue = ReturnType<typeof useAudioModuleState>;

const AudioModuleContext = createContext<AudioModuleContextValue | null>(null);

/**
 * Provides a single audio module state tree for the whole app.
 * @param props - Provider props.
 * @param props.children - Child tree.
 * @returns Provider element.
 */
export function AudioModuleProvider({ children }: { children: ReactNode }): React.JSX.Element {
  const value = useAudioModuleState();
  return <AudioModuleContext.Provider value={value}>{children}</AudioModuleContext.Provider>;
}

/**
 * Reads shared audio module state and actions from context.
 * @returns Audio module state and actions.
 */
export function useAudioModule(): AudioModuleContextValue {
  const context = useContext(AudioModuleContext);
  if (!context) {
    throw new Error("useAudioModule must be used within AudioModuleProvider");
  }
  return context;
}
