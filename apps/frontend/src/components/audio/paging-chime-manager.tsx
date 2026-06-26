import { useState } from "react";
import { CircleAlert, Loader2, Play, Trash2, Upload } from "lucide-react";

import { Badge } from "@/components/ui/badge.js";
import { Button } from "@/components/ui/button.js";
import type { UsePagingModuleResult } from "@/hooks/use-paging-module.js";

/**
 * Props for paging chime manager card.
 */
export interface PagingChimeManagerProps {
  /** Shared paging module hook result. */
  paging: UsePagingModuleResult;
}

/**
 * Formats optional chime duration for compact display.
 * @param durationMs - Duration in milliseconds.
 * @returns Human-readable duration or em dash.
 */
function formatDuration(durationMs: number | undefined): string {
  if (durationMs === undefined || durationMs <= 0) {
    return "—";
  }
  return `${(durationMs / 1000).toFixed(2)}s`;
}

/**
 * Chime manager for upload, preview, active selection, and deletion.
 * @param props - Component props.
 * @returns Chime manager card element.
 */
export function PagingChimeManager({ paging }: PagingChimeManagerProps): React.JSX.Element {
  const { state, previewChime, removeChime, setActiveChime, uploadChime } = paging;
  const [actionError, setActionError] = useState<string | null>(null);
  const [activeChimeAction, setActiveChimeAction] = useState<string | null>(null);

  return (
    <div className="rounded-lg border border-border bg-card">
      <div className="border-b border-border px-6 py-4">
        <h2 className="font-medium text-card-foreground">Chimes</h2>
        <p className="mt-0.5 text-sm text-muted-foreground">
          Upload WAV chimes, preview through house speakers, and choose the active default.
        </p>
        <p className="mt-1 text-xs text-muted-foreground">
          Preview turns zones on and routes them to source 8 (NOTIFICATIONS). Loudness is controlled
          per zone via Page Volume in Audio → Zones. The paging DAC analog output must be wired to
          that HiFi input (currently: GHW USB Audio, card 3).
        </p>
      </div>

      <div className="grid gap-4 p-6">
        {state.error || actionError ? (
          <div
            className="flex items-start gap-2 rounded-md border border-destructive/40 bg-destructive/10 px-3 py-2 text-sm text-destructive"
            role="alert"
          >
            <CircleAlert className="mt-0.5 size-4 shrink-0" aria-hidden />
            <p>{actionError ?? state.error}</p>
          </div>
        ) : null}

        <label className="grid gap-2">
          <span className="text-sm font-medium text-foreground">Upload custom chime (.wav)</span>
          <input
            type="file"
            accept=".wav,audio/wav"
            className="block w-full rounded-md border border-border bg-background px-3 py-2 text-sm text-foreground"
            onChange={(event) => {
              const file = event.target.files?.[0];
              if (!file) {
                return;
              }
              void (async () => {
                setActionError(null);
                setActiveChimeAction("__upload__");
                try {
                  await uploadChime(file);
                } catch (error) {
                  setActionError(error instanceof Error ? error.message : "Chime upload failed");
                } finally {
                  setActiveChimeAction(null);
                  event.target.value = "";
                }
              })();
            }}
          />
          <p className="text-xs text-muted-foreground">
            Max 500KB WAV recommended; backend validates duration and format.
          </p>
        </label>

        <div className="grid gap-3">
          {state.loadingChimes ? (
            <p className="text-sm text-muted-foreground">Loading chimes…</p>
          ) : state.chimes.length === 0 ? (
            <p className="text-sm text-muted-foreground">No chimes found.</p>
          ) : (
            state.chimes.map((chime) => {
              const actionBusy = activeChimeAction === chime.chimeId;
              return (
                <div
                  key={chime.chimeId}
                  className="grid gap-3 rounded-md border border-border/60 px-3 py-3"
                >
                  <div className="flex flex-wrap items-center gap-2">
                    <p className="text-sm font-medium text-foreground">{chime.displayName}</p>
                    {chime.isActive ? <Badge variant="secondary">Active</Badge> : null}
                    {chime.isBundled ? <Badge variant="outline">Bundled</Badge> : null}
                    <Badge variant="outline">Duration: {formatDuration(chime.durationMs)}</Badge>
                  </div>
                  <p className="break-all text-xs text-muted-foreground">{chime.chimeId}</p>
                  <div className="flex flex-wrap gap-2">
                    <Button
                      type="button"
                      variant="outline"
                      size="sm"
                      disabled={actionBusy}
                      onClick={() => {
                        void (async () => {
                          setActionError(null);
                          setActiveChimeAction(chime.chimeId);
                          try {
                            await previewChime(chime.chimeId);
                          } catch (error) {
                            setActionError(error instanceof Error ? error.message : "Preview failed");
                          } finally {
                            setActiveChimeAction(null);
                          }
                        })();
                      }}
                    >
                      {actionBusy ? (
                        <Loader2 className="size-4 animate-spin" />
                      ) : (
                        <Play className="size-4" />
                      )}
                      Preview
                    </Button>
                    {!chime.isActive ? (
                      <Button
                        type="button"
                        variant="outline"
                        size="sm"
                        disabled={actionBusy}
                        onClick={() => {
                          void (async () => {
                            setActionError(null);
                            setActiveChimeAction(chime.chimeId);
                            try {
                              await setActiveChime(chime.chimeId);
                            } catch (error) {
                              setActionError(
                                error instanceof Error ? error.message : "Set active chime failed"
                              );
                            } finally {
                              setActiveChimeAction(null);
                            }
                          })();
                        }}
                      >
                        {actionBusy ? <Loader2 className="size-4 animate-spin" /> : <Upload className="size-4" />}
                        Set Active
                      </Button>
                    ) : null}
                    <Button
                      type="button"
                      variant="destructive"
                      size="sm"
                      disabled={actionBusy || chime.isBundled}
                      onClick={() => {
                        void (async () => {
                          setActionError(null);
                          setActiveChimeAction(chime.chimeId);
                          try {
                            await removeChime(chime.chimeId);
                          } catch (error) {
                            setActionError(error instanceof Error ? error.message : "Delete failed");
                          } finally {
                            setActiveChimeAction(null);
                          }
                        })();
                      }}
                    >
                      {actionBusy ? <Loader2 className="size-4 animate-spin" /> : <Trash2 className="size-4" />}
                      Delete
                    </Button>
                  </div>
                </div>
              );
            })
          )}
        </div>
      </div>
    </div>
  );
}
