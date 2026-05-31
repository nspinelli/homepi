import { Outlet } from "react-router-dom";

import { Navbar } from "@/components/navbar.js";
import { Toaster } from "@/components/ui/toaster.js";

/**
 * Application shell with a fixed sticky header and scrollable page content.
 */
export function AppLayout(): React.JSX.Element {
  return (
    <div className="min-h-screen overflow-x-hidden bg-background">
      <Navbar />
      <div className="pt-14">
        <Outlet />
      </div>
      <Toaster />
    </div>
  );
}
