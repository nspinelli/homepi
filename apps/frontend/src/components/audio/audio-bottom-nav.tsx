import { ArrowLeft } from "lucide-react";
import { createPortal } from "react-dom";
import { useNavigate } from "react-router-dom";

import { AudioSectionTabsList } from "@/components/audio/audio-section-tabs.js";
import { Button } from "@/components/ui/button.js";
import { cn } from "@/lib/utils.js";

/**
 * Fixed bottom navigation with back affordance and audio section tabs.
 * Portaled to document.body so it stays pinned to the viewport while content scrolls.
 */
export function AudioBottomNav(): React.JSX.Element {
  const navigate = useNavigate();

  return createPortal(
    <div
      className="pointer-events-none fixed inset-x-0 bottom-0 z-40 flex justify-center px-4 pb-[max(1rem,env(safe-area-inset-bottom))]"
      aria-hidden={false}
    >
      <div className="pointer-events-auto flex max-w-full items-center gap-2">
        <Button
          type="button"
          variant="ghost"
          size="sm"
          aria-label="Go back"
          onClick={() => navigate(-1)}
          className={cn(
            "h-auto min-w-[4.25rem] shrink-0 flex-col gap-1 rounded-full border border-border/60",
            "bg-audio-tabs-track/95 px-2.5 py-2.5 text-[11px] font-medium leading-tight shadow-lg backdrop-blur-md",
            "text-muted-foreground hover:bg-audio-tabs-track hover:text-foreground"
          )}
        >
          <ArrowLeft className="h-5 w-5 shrink-0" strokeWidth={2} aria-hidden />
          <span>Back</span>
        </Button>
        <AudioSectionTabsList />
      </div>
    </div>,
    document.body
  );
}
