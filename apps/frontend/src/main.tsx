import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { App } from "./app.js";
import { UserSettingsProvider } from "./hooks/use-user-settings.js";
import "./styles/globals.css";

const root = document.getElementById("root");
if (!root) {
  throw new Error("Root element not found");
}

createRoot(root).render(
  <StrictMode>
    <UserSettingsProvider>
      <App />
    </UserSettingsProvider>
  </StrictMode>
);
