import { AudioSectionTabsList } from "@/components/audio/audio-section-tabs.js";

/**
 * Fixed bottom navigation with audio section tabs.
 */
export function AudioBottomNav(): React.JSX.Element {
  return (
    <div
      className="pointer-events-none fixed inset-x-0 bottom-0 z-40 flex justify-center px-4 pb-[max(1rem,env(safe-area-inset-bottom))]"
      aria-hidden={false}
    >
      <AudioSectionTabsList className="pointer-events-auto" />
    </div>
  );
}
