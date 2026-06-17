import { Link } from "react-router-dom";

import { NowPlayingHeader } from "@/components/now-playing-header.js";
import { StatusHeaderButton } from "@/components/status-header-button.js";
import { ThemeHeaderButton } from "@/components/theme-header-button.js";

/**
 * Sticky top navigation bar with a translucent background so scrolling content passes underneath.
 */
export function Navbar(): React.JSX.Element {
  return (
    <header className="fixed top-0 z-50 w-full border-b border-border/60 bg-header/75 backdrop-blur-xl supports-[backdrop-filter]:bg-header/65">
      <div className="mx-auto flex h-14 max-w-4xl items-center justify-between px-4">
        <Link to="/" className="flex shrink-0 items-center gap-2">
          <img
            src="/homepi-logo.png"
            alt="HomePi"
            width={32}
            height={32}
            className="size-8 rounded-lg"
          />
          <span className="text-lg font-semibold text-foreground">HomePi</span>
        </Link>
        <nav className="flex min-w-0 flex-1 items-center justify-end gap-2 sm:gap-3">
          <ThemeHeaderButton />
          <StatusHeaderButton />
          <div className="pl-1 sm:pl-2">
            <NowPlayingHeader />
          </div>
        </nav>
      </div>
    </header>
  );
}
