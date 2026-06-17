import { ChevronRight } from "lucide-react";
import { Link } from "react-router-dom";

import { Badge } from "@/components/ui/badge.js";
import { Card, CardContent } from "@/components/ui/card.js";
import { cn } from "@/lib/utils.js";

/**
 * Props for the home dashboard audio module card.
 */
export interface AudioCardProps {
  /** Display name for the audio system. */
  name: string;
  /** Whether the Hi-Fi controller serial link is up. */
  isConnected: boolean;
  /** Optional now-playing track title. */
  currentTrack?: string;
  /** Optional artist name. */
  artist?: string;
  /** Optional album name. */
  album?: string;
  /** Optional source label. */
  source?: string;
  /** Optional album cover URL. */
  coverUrl?: string;
  /** Service status labels when not playing. */
  serviceStatuses?: { label: string; status: string }[];
}

/**
 * Dashboard card for the audio module; links to /audio.
 */
export function AudioCard({
  name,
  isConnected,
  currentTrack,
  artist,
  album,
  source,
  coverUrl,
  serviceStatuses = [],
}: AudioCardProps): React.JSX.Element {
  const showNowPlaying = isConnected && Boolean(currentTrack);

  return (
    <Link to="/audio" className="block">
      <Card className="group cursor-pointer transition-colors hover:bg-secondary/50">
        <CardContent className="p-6">
          <div className="flex items-start justify-between">
            <div className="flex items-center gap-4">
              <img
                src="/audio-controller.png"
                alt=""
                width={64}
                height={64}
                className="h-16 w-16 shrink-0 object-contain"
              />
              <div>
                <h3 className="text-lg font-semibold text-foreground">{name}</h3>
                <div className="mt-1.5 flex items-center gap-2">
                  <span
                    className={cn(
                      "h-2 w-2 shrink-0 rounded-full",
                      isConnected ? "bg-success" : "bg-muted-foreground"
                    )}
                    aria-hidden
                  />
                  <span
                    className={cn(
                      "text-sm",
                      isConnected ? "text-success" : "text-muted-foreground"
                    )}
                  >
                    {isConnected ? "Connected" : "Disconnected"}
                  </span>
                </div>
              </div>
            </div>
            <ChevronRight className="h-5 w-5 text-muted-foreground transition-transform group-hover:translate-x-1" />
          </div>

          {showNowPlaying ? (
            <div className="mt-6 border-t border-border pt-4">
              <p className="mb-3 text-xs font-medium tracking-wider text-muted-foreground uppercase">
                Now Playing
              </p>
              <div className="flex items-center gap-4">
                {coverUrl ? (
                  <img
                    src={coverUrl}
                    alt=""
                    width={56}
                    height={56}
                    className="h-14 w-14 shrink-0 rounded object-cover"
                  />
                ) : null}
                <div className="min-w-0 flex-1">
                  <p className="truncate text-sm font-medium text-foreground">{currentTrack}</p>
                  {artist ? (
                    <p className="truncate text-sm text-muted-foreground">{artist}</p>
                  ) : null}
                  {album ? (
                    <p className="truncate text-xs text-muted-foreground">{album}</p>
                  ) : null}
                  {source ? (
                    <Badge variant="outline" className="mt-2 text-xs">
                      {source}
                    </Badge>
                  ) : null}
                </div>
              </div>
            </div>
          ) : (
            <div className="mt-6 border-t border-border pt-4">
              <p className="mb-2 text-xs font-medium tracking-wider text-muted-foreground uppercase">
                Services
              </p>
              <div className="flex flex-wrap gap-2">
                {serviceStatuses.map((item) => (
                  <Badge
                    key={item.label}
                    variant="outline"
                    className={
                      item.status === "healthy"
                        ? "border-success/40 text-success"
                        : item.status === "degraded"
                          ? "border-warning/40 text-warning"
                          : ""
                    }
                  >
                    {item.label}: {item.status}
                  </Badge>
                ))}
              </div>
            </div>
          )}
        </CardContent>
      </Card>
    </Link>
  );
}
