import { BrowserRouter, Route, Routes } from "react-router-dom";

import { AppLayout } from "@/components/app-layout.js";
import { HomePage } from "@/pages/home-page.js";
import { SettingsPage } from "@/pages/settings-page.js";
import { StatusPage } from "@/pages/status-page.js";

/**
 * HomePi frontend application shell with routed pages.
 */
export function App(): React.JSX.Element {
  return (
    <BrowserRouter>
      <Routes>
        <Route element={<AppLayout />}>
          <Route index element={<HomePage />} />
          <Route path="status" element={<StatusPage />} />
          <Route path="settings" element={<SettingsPage />} />
        </Route>
      </Routes>
    </BrowserRouter>
  );
}
