import type { LucideIcon } from "lucide-react";
import { Megaphone, Radio, Settings, Speaker } from "lucide-react";

import { TabsList, TabsTrigger } from "@/components/ui/tabs.js";
import { cn } from "@/lib/utils.js";

/**
 * Audio module tab definition for the pill navigation bar.
 */
export interface AudioSectionTabItem {
  /** Radix tab value. */
  value: string;
  /** Visible label under the icon. */
  label: string;
  /** Lucide icon component. */
  icon: LucideIcon;
}

/** Tab entries shown on the Home Audio page. */
export const AUDIO_SECTION_TABS: AudioSectionTabItem[] = [
  { value: "zones", label: "Zones", icon: Speaker },
  { value: "sources", label: "Sources", icon: Radio },
  { value: "paging", label: "Paging", icon: Megaphone },
  { value: "settings", label: "Settings", icon: Settings },
];

/**
 * Props for the floating audio section tab bar.
 */
export interface AudioSectionTabsListProps {
  /** Optional class names for the tab list container. */
  className?: string;
}

/**
 * Pill-shaped tab list for the audio module (icon above label).
 * @param props - Optional styling overrides.
 */
export function AudioSectionTabsList({
  className,
}: AudioSectionTabsListProps): React.JSX.Element {
  return (
    <TabsList
      className={cn(
        "inline-flex h-auto w-auto max-w-[calc(100vw-2rem)] gap-0.5 rounded-full p-1",
        "border border-border/60 bg-audio-tabs-track/95 shadow-lg backdrop-blur-md",
        "overflow-x-auto [-ms-overflow-style:none] [scrollbar-width:none] [&::-webkit-scrollbar]:hidden",
        className
      )}
    >
      {AUDIO_SECTION_TABS.map((tab) => (
        <AudioSectionTabTrigger key={tab.value} tab={tab} />
      ))}
    </TabsList>
  );
}

/**
 * Single audio section tab trigger with stacked icon and label.
 * @param props - Tab metadata.
 */
function AudioSectionTabTrigger({ tab }: { tab: AudioSectionTabItem }): React.JSX.Element {
  const Icon = tab.icon;

  return (
    <TabsTrigger
      value={tab.value}
      className={cn(
        "min-w-[4.25rem] shrink-0 flex-col gap-1 rounded-full border-0 px-2.5 py-2.5",
        "h-auto text-[11px] font-medium leading-tight shadow-none",
        "text-muted-foreground transition-colors",
        "focus-visible:ring-2 focus-visible:ring-zone-accent/40",
        "data-[state=active]:bg-audio-tabs-active data-[state=active]:text-zone-accent",
        "data-[state=active]:shadow-none",
        "dark:text-foreground/75 dark:data-[state=active]:text-zone-accent"
      )}
    >
      <Icon className="h-5 w-5 shrink-0" strokeWidth={2} aria-hidden />
      <span className="truncate">{tab.label}</span>
    </TabsTrigger>
  );
}
