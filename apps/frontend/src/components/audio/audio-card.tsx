import { ChevronRight } from "lucide-react";
import { Link } from "react-router-dom";

import { Card, CardContent } from "@/components/ui/card.js";
import {
  audioConnectionLabel,
  type AudioConnectionLevel,
} from "@/lib/derive-audio-connection-level.js";
import { cn } from "@/lib/utils.js";

/**
 * Props for the home dashboard audio module card.
 */
export interface AudioCardProps {
  /** Display name for the audio system. */
  name: string;
  /** Aggregate connection health for the status pill. */
  connectionLevel: AudioConnectionLevel;
}

const PILL_DOT_CLASS: Record<AudioConnectionLevel, string> = {
  healthy: "bg-success",
  degraded: "bg-warning",
  offline: "bg-destructive",
};

const PILL_BG_CLASS: Record<AudioConnectionLevel, string> = {
  healthy: "bg-success/15",
  degraded: "bg-warning/15",
  offline: "bg-destructive/15",
};

/**
 * Dashboard card for the audio module; links to /audio.
 */
export function AudioCard({ name, connectionLevel }: AudioCardProps): React.JSX.Element {
  const statusLabel = audioConnectionLabel(connectionLevel);

  return (
    <Link to="/audio" className="block">
      <Card className="group cursor-pointer transition-colors hover:bg-secondary/50">
        <CardContent className="px-6 py-3">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-4">
              <img
                src="/audio-controller.png"
                alt=""
                width={112}
                height={112}
                className="h-28 w-28 shrink-0 object-contain"
              />
              <div>
                <h3 className="text-lg font-semibold text-foreground">{name}</h3>
                <span
                  className={cn(
                    "mt-1.5 inline-flex items-center gap-1.5 rounded-full px-2.5 py-0.5 text-xs font-medium text-foreground",
                    PILL_BG_CLASS[connectionLevel]
                  )}
                >
                  <span
                    className={cn("h-1.5 w-1.5 shrink-0 rounded-full", PILL_DOT_CLASS[connectionLevel])}
                    aria-hidden
                  />
                  {statusLabel}
                </span>
              </div>
            </div>
            <ChevronRight className="h-5 w-5 text-muted-foreground transition-transform group-hover:translate-x-1" />
          </div>
        </CardContent>
      </Card>
    </Link>
  );
}
