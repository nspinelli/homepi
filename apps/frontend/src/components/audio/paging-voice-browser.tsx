import { useMemo, useState } from "react";
import { CircleAlert, Download, ExternalLink, Loader2, Play, Trash2 } from "lucide-react";

import { Badge } from "@/components/ui/badge.js";
import { Button } from "@/components/ui/button.js";
import { Input } from "@/components/ui/input.js";
import type { UsePagingModuleResult } from "@/hooks/use-paging-module.js";
import type { PagingVoice } from "@/types/paging-types.js";

/**
 * Props for paging voice browser card.
 */
export interface PagingVoiceBrowserProps {
  /** Shared paging module hook result. */
  paging: UsePagingModuleResult;
}

/**
 * Builds a readable label for a catalog voice option.
 * @param voice - Catalog voice row.
 * @returns Dropdown label text.
 */
function voiceOptionLabel(voice: PagingVoice): string {
  const accent = voice.accent ?? voice.languageCode;
  const status = voice.installed ? "installed" : "not installed";
  return `${voice.displayName} (${accent}, ${voice.quality}) — ${status}`;
}

/**
 * Voice catalog browser with English download dropdown and installed voice management.
 * @param props - Component props.
 * @returns Voice browser card element.
 */
export function PagingVoiceBrowser({ paging }: PagingVoiceBrowserProps): React.JSX.Element {
  const { state, installVoice, previewVoice, removeVoice, setDefaultVoice } = paging;
  const [previewText, setPreviewText] = useState("This is a HomePi paging preview.");
  const [actionError, setActionError] = useState<string | null>(null);
  const [activeVoiceAction, setActiveVoiceAction] = useState<string | null>(null);
  const [selectedDownloadVoiceId, setSelectedDownloadVoiceId] = useState("");

  const maxPreviewLength = state.config?.maxPreviewTextLength ?? 120;
  const maxInstalled = state.config?.maxInstalledVoices ?? 2;

  const englishVoices = useMemo(
    () =>
      state.voices.filter(
        (voice) =>
          voice.languageCode.toLowerCase().startsWith("en") || voice.voiceId.startsWith("en_")
      ),
    [state.voices]
  );

  const installedVoices = useMemo(
    () => englishVoices.filter((voice) => voice.installed),
    [englishVoices]
  );

  const downloadableVoices = useMemo(
    () => englishVoices.filter((voice) => !voice.installed),
    [englishVoices]
  );

  const defaultSelectedDownloadId = useMemo(() => {
    if (downloadableVoices.length === 0) {
      return "";
    }
    if (
      selectedDownloadVoiceId &&
      downloadableVoices.some((voice) => voice.voiceId === selectedDownloadVoiceId)
    ) {
      return selectedDownloadVoiceId;
    }
    return downloadableVoices[0]?.voiceId ?? "";
  }, [downloadableVoices, selectedDownloadVoiceId]);

  const effectiveDownloadVoiceId = selectedDownloadVoiceId || defaultSelectedDownloadId;
  const effectiveDownloadVoice =
    englishVoices.find((voice) => voice.voiceId === effectiveDownloadVoiceId) ?? null;
  const installSlotsFull = installedVoices.length >= maxInstalled;

  return (
    <div className="rounded-lg border border-border bg-card">
      <div className="border-b border-border px-6 py-4">
        <h2 className="font-medium text-card-foreground">Voices</h2>
        <p className="mt-0.5 text-sm text-muted-foreground">
          Download English Piper voices, set your default, and preview through the paging DAC.
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

        <div className="rounded-md border border-border/60 px-3 py-2 text-xs text-muted-foreground">
          Installed voices: {installedVoices.length}/{maxInstalled}
        </div>

        <div className="grid gap-3 rounded-md border border-border/60 p-4">
          <p className="text-sm font-medium text-foreground">Download a voice</p>
          {state.loadingVoices ? (
            <p className="text-sm text-muted-foreground">Loading English voice catalog…</p>
          ) : englishVoices.length === 0 ? (
            <p className="text-sm text-muted-foreground">
              No English voices returned by the catalog endpoint.
            </p>
          ) : downloadableVoices.length === 0 ? (
            <p className="text-sm text-muted-foreground">
              All available English catalog voices are already installed.
            </p>
          ) : (
            <>
              <label className="grid gap-2">
                <span className="text-sm text-muted-foreground">Available English voices</span>
                <select
                  className="h-10 w-full rounded-md border border-border bg-background px-3 text-sm text-foreground"
                  value={effectiveDownloadVoiceId}
                  onChange={(event) => setSelectedDownloadVoiceId(event.target.value)}
                >
                  {downloadableVoices.map((voice) => (
                    <option key={voice.voiceId} value={voice.voiceId}>
                      {voiceOptionLabel(voice)}
                    </option>
                  ))}
                </select>
              </label>

              <div className="flex flex-wrap gap-2">
                <Button
                  type="button"
                  size="sm"
                  disabled={
                    !effectiveDownloadVoice ||
                    installSlotsFull ||
                    activeVoiceAction === effectiveDownloadVoice.voiceId
                  }
                  onClick={() => {
                    if (!effectiveDownloadVoice) {
                      return;
                    }
                    void (async () => {
                      setActionError(null);
                      setActiveVoiceAction(effectiveDownloadVoice.voiceId);
                      try {
                        await installVoice(effectiveDownloadVoice.voiceId, false);
                      } catch (error) {
                        setActionError(
                          error instanceof Error ? error.message : "Download failed"
                        );
                      } finally {
                        setActiveVoiceAction(null);
                      }
                    })();
                  }}
                >
                  {activeVoiceAction === effectiveDownloadVoice?.voiceId ? (
                    <Loader2 className="size-4 animate-spin" />
                  ) : (
                    <Download className="size-4" />
                  )}
                  Download voice
                </Button>

                {effectiveDownloadVoice?.sampleAvailable && effectiveDownloadVoice.sampleUrl ? (
                  <Button type="button" variant="outline" size="sm" asChild>
                    <a href={effectiveDownloadVoice.sampleUrl} target="_blank" rel="noreferrer">
                      <ExternalLink className="size-4" />
                      Listen to sample
                    </a>
                  </Button>
                ) : null}
              </div>

              {installSlotsFull ? (
                <p className="text-xs text-amber-700 dark:text-amber-300">
                  Remove an installed voice before downloading another.
                </p>
              ) : null}
            </>
          )}
        </div>

        <label className="grid gap-2">
          <span className="text-sm font-medium text-foreground">DAC preview text</span>
          <Input
            value={previewText}
            maxLength={maxPreviewLength}
            onChange={(event) => setPreviewText(event.target.value)}
          />
          <span className="text-xs text-muted-foreground">
            {previewText.length}/{maxPreviewLength} characters
          </span>
        </label>

        <div className="grid gap-3">
          <p className="text-sm font-medium text-foreground">Installed voices</p>
          {installedVoices.length === 0 ? (
            <p className="text-sm text-muted-foreground">
              No installed voices yet. Choose one from the download list above.
            </p>
          ) : (
            installedVoices.map((voice) => {
              const actionBusy = activeVoiceAction === voice.voiceId;
              return (
                <div
                  key={voice.voiceId}
                  className="grid gap-3 rounded-md border border-border/60 px-3 py-3"
                >
                  <div className="flex flex-wrap items-center gap-2">
                    <p className="text-sm font-medium text-foreground">{voice.displayName}</p>
                    <Badge variant="outline">{voice.accent ?? voice.languageCode}</Badge>
                    <Badge variant="outline">{voice.quality}</Badge>
                    <Badge>Installed</Badge>
                    {voice.isDefault ? <Badge variant="secondary">Default</Badge> : null}
                    {voice.isBundled ? <Badge variant="outline">Bundled</Badge> : null}
                  </div>
                  <p className="break-all text-xs text-muted-foreground">{voice.voiceId}</p>
                  <div className="flex flex-wrap gap-2">
                    <Button
                      type="button"
                      variant="outline"
                      size="sm"
                      disabled={actionBusy || previewText.trim().length === 0}
                      onClick={() => {
                        void (async () => {
                          setActionError(null);
                          setActiveVoiceAction(voice.voiceId);
                          try {
                            await previewVoice(voice.voiceId, previewText.trim());
                          } catch (error) {
                            setActionError(
                              error instanceof Error ? error.message : "Preview failed"
                            );
                          } finally {
                            setActiveVoiceAction(null);
                          }
                        })();
                      }}
                    >
                      {actionBusy ? (
                        <Loader2 className="size-4 animate-spin" />
                      ) : (
                        <Play className="size-4" />
                      )}
                      Preview on DAC
                    </Button>
                    {!voice.isDefault ? (
                      <Button
                        type="button"
                        variant="outline"
                        size="sm"
                        disabled={actionBusy}
                        onClick={() => {
                          void (async () => {
                            setActionError(null);
                            setActiveVoiceAction(voice.voiceId);
                            try {
                              await setDefaultVoice(voice.voiceId);
                            } catch (error) {
                              setActionError(
                                error instanceof Error ? error.message : "Set default failed"
                              );
                            } finally {
                              setActiveVoiceAction(null);
                            }
                          })();
                        }}
                      >
                        {actionBusy ? <Loader2 className="size-4 animate-spin" /> : "Set Default"}
                      </Button>
                    ) : null}
                    <Button
                      type="button"
                      variant="destructive"
                      size="sm"
                      disabled={actionBusy || voice.isDefault}
                      title={voice.isDefault ? "Set another voice as default before removing this one." : undefined}
                      onClick={() => {
                        void (async () => {
                          setActionError(null);
                          setActiveVoiceAction(voice.voiceId);
                          try {
                            await removeVoice(voice.voiceId);
                          } catch (error) {
                            setActionError(
                              error instanceof Error ? error.message : "Remove failed"
                            );
                          } finally {
                            setActiveVoiceAction(null);
                          }
                        })();
                      }}
                    >
                      {actionBusy ? (
                        <Loader2 className="size-4 animate-spin" />
                      ) : (
                        <Trash2 className="size-4" />
                      )}
                      Remove
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
