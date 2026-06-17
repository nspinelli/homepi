import { Link } from "react-router-dom";

import { NowPlayingHeader } from "@/components/now-playing-header.js";
import { StatusHeaderButton } from "@/components/status-header-button.js";
import { ThemeHeaderButton } from "@/components/theme-header-button.js";

/**
 * Sticky top navigation bar with a solid background so scrolling content is hidden behind it.
 */
export function Navbar(): React.JSX.Element {
  return (
    <header className="fixed top-0 z-50 w-full border-b border-border/60 bg-header">
      <div className="mx-auto grid h-14 max-w-4xl grid-cols-[1fr_auto_1fr] items-center px-4">
        <Link to="/" className="flex shrink-0 items-center gap-2 justify-self-start">
          <img
            src="/homepi-logo.png"
            alt="HomePi"
            width={32}
            height={32}
            className="size-8 rounded-lg"
          />
          <span className="text-lg font-semibold text-foreground">HomePi</span>
        </Link>

        <div className="flex min-w-0 justify-center justify-self-center px-2">
          <NowPlayingHeader />
        </div>

        <nav className="flex items-center justify-end gap-2 sm:gap-3 justify-self-end">
          <ThemeHeaderButton />
          <StatusHeaderButton />
        </nav>
      </div>
    </header>
  );
}
