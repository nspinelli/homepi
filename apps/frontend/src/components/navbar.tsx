import { Activity, Settings } from "lucide-react";
import { Link, NavLink } from "react-router-dom";

import { Button } from "@/components/ui/button.js";
import { cn } from "@/lib/utils.js";

/**
 * Sticky top navigation bar with a translucent background so scrolling content passes underneath.
 */
export function Navbar(): React.JSX.Element {
  return (
    <header className="fixed top-0 z-50 w-full border-b border-border/60 bg-header/75 backdrop-blur-xl supports-[backdrop-filter]:bg-header/65">
      <div className="mx-auto flex h-14 max-w-4xl items-center justify-between px-4">
        <Link to="/" className="flex items-center gap-2">
          <img
            src="/homepi-logo.png"
            alt="HomePi"
            width={32}
            height={32}
            className="size-8 rounded-lg"
          />
          <span className="text-lg font-semibold text-foreground">HomePi</span>
        </Link>
        <nav className="flex items-center gap-2">
          <Button variant="ghost" size="sm" className="gap-2" asChild>
            <NavLink
              to="/status"
              className={({ isActive }) =>
                cn(isActive && "bg-accent/10 text-accent-foreground")
              }
            >
              <Activity className="size-4" />
              <span className="hidden sm:inline">Status</span>
            </NavLink>
          </Button>
          <Button variant="ghost" size="sm" className="gap-2" asChild>
            <NavLink
              to="/settings"
              className={({ isActive }) =>
                cn(isActive && "bg-accent/10 text-accent-foreground")
              }
            >
              <Settings className="size-4" />
              <span className="hidden sm:inline">Settings</span>
            </NavLink>
          </Button>
        </nav>
      </div>
    </header>
  );
}
