import { Outlet } from "react-router-dom";

import { Navbar } from "@/components/navbar.js";

/**
 * Application shell with a fixed sticky header and scrollable page content.
 */
export function AppLayout(): React.JSX.Element {
  return (
    <div className="min-h-screen bg-background">
      <Navbar />
      <Outlet />
    </div>
  );
}
