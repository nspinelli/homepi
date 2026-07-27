import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { App } from "./app.js";
import { AudioModuleProvider } from "./hooks/audio-module-provider.js";
import { SensorsModuleProvider } from "./hooks/sensors-module-provider.js";
import { SystemDashboardProvider } from "./hooks/system-dashboard-provider.js";
import { UserSettingsProvider } from "./hooks/use-user-settings.js";
import "./styles/globals.css";

const root = document.getElementById("root");
if (!root) {
  throw new Error("Root element not found");
}

createRoot(root).render(
  <StrictMode>
    <UserSettingsProvider>
      <SystemDashboardProvider>
        <AudioModuleProvider>
          <SensorsModuleProvider>
            <App />
          </SensorsModuleProvider>
        </AudioModuleProvider>
      </SystemDashboardProvider>
    </UserSettingsProvider>
  </StrictMode>
);
