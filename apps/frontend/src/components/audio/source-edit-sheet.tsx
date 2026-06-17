import { Airplay, Radio, X } from "lucide-react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { createPortal } from "react-dom";

import { ZoneEditSlider } from "@/components/audio/zone-edit-slider.js";
import {
  buildSourceSettingsPatch,
  hasSourceSettingsChanges,
  sourceSettingsSnapshotKey,
  sourceToFormState,
  type SourceSettingsFormState,
} from "@/components/audio/source-settings-patch.js";
import { Button } from "@/components/ui/button.js";
import { Switch } from "@/components/ui/switch.js";
import { cn } from "@/lib/utils.js";
import type { HifiSource, SourceSettingsPatch } from "@/types/audio-types.js";

/**
 * Props for the iOS-style source edit bottom sheet.
 */
export interface SourceEditSheetProps {
  /** Whether the sheet is open. */
  open: boolean;
  /** Open state change handler. */
  onOpenChange: (open: boolean) => void;
  /** Source row from snapshot. */
  source: HifiSource | null;
  /** Whether a save is in progress. */
  saving: boolean;
  /** Persists source settings. Rejects when the save request fails. */
  onSave: (sourceNumber: number, patch: SourceSettingsPatch) => Promise<void>;
}

/**
 * Section header label for sheet form groups.
 */
function SectionHeader({ title }: { title: string }): React.JSX.Element {
  return (
    <p className="text-xs font-semibold tracking-wide text-muted-foreground uppercase">{title}</p>
  );
}

/**
 * Rounded card wrapper used inside the sheet body.
 */
function SheetCard({
  children,
  className,
}: {
  children: React.ReactNode;
  className?: string;
}): React.JSX.Element {
  return (
    <div
      className={cn(
        "rounded-2xl border border-border/60 bg-card p-4 shadow-sm",
        className
      )}
    >
      {children}
    </div>
  );
}

/**
 * iOS-style bottom sheet for editing Hi-Fi2 source settings.
 */
export function SourceEditSheet({
  open,
  onOpenChange,
  source,
  saving,
  onSave,
}: SourceEditSheetProps): React.JSX.Element | null {
  const [form, setForm] = useState<SourceSettingsFormState | null>(() =>
    source ? sourceToFormState(source) : null
  );
  const sourceSnapshotKey = useMemo(
    () => (source ? sourceSettingsSnapshotKey(source) : ""),
    [source]
  );

  const openedSourceRef = useRef<number | null>(null);

  useEffect(() => {
    if (!open || !source) {
      if (!open) {
        openedSourceRef.current = null;
      }
      return;
    }

    const isNewSession = openedSourceRef.current !== source.sourceNumber;
    openedSourceRef.current = source.sourceNumber;

    if (isNewSession) {
      setForm(sourceToFormState(source));
      return;
    }

    setForm((current) => {
      if (current && hasSourceSettingsChanges(source, current)) {
        return current;
      }
      return sourceToFormState(source);
    });
  }, [open, source, sourceSnapshotKey]);

  const updateForm = useCallback(
    (partial: Partial<SourceSettingsFormState>) => {
      setForm((current) => {
        const base = current ?? (source ? sourceToFormState(source) : null);
        if (!base) {
          return current;
        }
        return { ...base, ...partial };
      });
    },
    [source]
  );

  const unsaved = useMemo(() => {
    if (!source) {
      return false;
    }
    const activeForm = form ?? sourceToFormState(source);
    return hasSourceSettingsChanges(source, activeForm);
  }, [source, form]);

  const handleClose = useCallback(() => {
    onOpenChange(false);
  }, [onOpenChange]);

  useEffect(() => {
    if (!open) {
      return;
    }

    const previousOverflow = document.body.style.overflow;
    document.body.style.overflow = "hidden";

    const onKeyDown = (event: KeyboardEvent): void => {
      if (event.key === "Escape") {
        onOpenChange(false);
      }
    };
    document.addEventListener("keydown", onKeyDown);

    return () => {
      document.body.style.overflow = previousOverflow;
      document.removeEventListener("keydown", onKeyDown);
    };
  }, [open, onOpenChange]);

  const handleSave = useCallback(async () => {
    if (!source || saving) {
      return;
    }
    const activeForm = form ?? sourceToFormState(source);
    const patch = buildSourceSettingsPatch(source, activeForm);
    if (!patch) {
      handleClose();
      return;
    }
    try {
      await onSave(source.sourceNumber, patch);
      handleClose();
    } catch {
      /* Toast shown by saveSourceSettings; keep sheet open for corrections. */
    }
  }, [source, form, saving, onSave, handleClose]);

  if (!open || !source) {
    return null;
  }

  const sheetForm = form ?? sourceToFormState(source);
  const isCurrentAirplay = source.isAirplay === 1;
  const airplaySwitchDisabled =
    isCurrentAirplay || sheetForm.enabled !== 1;

  return createPortal(
    <>
      <button
        type="button"
        className="fixed inset-0 z-[200] bg-black/40 backdrop-blur-[2px]"
        aria-label="Close edit source"
        onClick={handleClose}
      />

      <div
        role="dialog"
        aria-modal="true"
        aria-labelledby="source-edit-sheet-title"
        className={cn(
          "fixed inset-x-0 bottom-0 z-[210] mx-auto flex w-full max-w-lg flex-col overflow-hidden",
          "max-h-[min(92vh,900px)] min-h-[50vh]",
          "rounded-t-[1.75rem] border border-border/60 bg-card text-card-foreground shadow-2xl"
        )}
        onClick={(event) => event.stopPropagation()}
      >
        <div className="flex shrink-0 justify-center pt-3 pb-1">
          <div className="h-1 w-10 rounded-full bg-muted-foreground/30" aria-hidden />
        </div>

        <div className="relative flex min-h-0 flex-1 flex-col overflow-hidden">
          <header className="absolute inset-x-0 top-0 z-20 flex items-center gap-3 border-b border-border/60 bg-card/80 px-4 py-3 shadow-sm backdrop-blur-xl supports-[backdrop-filter]:bg-card/65">
            <Button
              type="button"
              variant="ghost"
              size="icon"
              className="h-10 w-10 shrink-0 rounded-full bg-muted/70"
              aria-label="Close"
              onClick={handleClose}
            >
              <X className="h-5 w-5 text-muted-foreground" />
            </Button>

            <div className="min-w-0 flex-1 text-center">
              <h2 id="source-edit-sheet-title" className="text-base font-semibold text-foreground">
                Edit Source
              </h2>
              <span className="mt-1 inline-block rounded-full bg-zone-accent/15 px-2.5 py-0.5 text-xs font-medium text-zone-accent">
                Source {source.sourceNumber}
              </span>
            </div>

            <Button
              type="button"
              size="sm"
              className="shrink-0 rounded-full bg-zone-accent px-4 text-white hover:bg-zone-accent/90"
              disabled={saving || !unsaved}
              onClick={() => void handleSave()}
            >
              {saving ? "Saving…" : "Save"}
            </Button>
          </header>

          <div className="min-h-0 flex-1 overflow-y-auto overscroll-contain pt-[4.75rem]">
            <div className="space-y-6 px-4 pt-2 pb-8">
              <SheetCard>
                <div className="flex gap-4">
                  <div className="flex h-14 w-14 shrink-0 items-center justify-center rounded-2xl bg-zone-accent/15">
                    <Radio className="h-7 w-7 text-zone-accent" />
                  </div>
                  <div className="min-w-0 flex-1">
                    <p className="text-sm font-medium text-foreground">Preview</p>
                    <p className="text-xs text-muted-foreground">How this source appears in the UI</p>
                  </div>
                </div>
                <p className="mt-4 text-lg font-semibold text-foreground">{sheetForm.name}</p>
                {sheetForm.displayLine ? (
                  <p className="mt-1 text-sm text-muted-foreground">{sheetForm.displayLine}</p>
                ) : null}
              </SheetCard>

              <div className="space-y-3">
                <SectionHeader title="General" />
                <SheetCard className="space-y-4">
                  <label className="grid gap-2">
                    <span className="text-sm font-medium text-foreground">Name</span>
                    <input
                      type="text"
                      className="h-10 rounded-xl border border-border bg-background px-3 text-sm text-foreground"
                      value={sheetForm.name}
                      onChange={(event) => updateForm({ name: event.target.value })}
                    />
                  </label>

                  <div className="flex items-center justify-between gap-4">
                    <div>
                      <p className="text-sm font-medium text-foreground">Enabled</p>
                      <p className="text-xs text-muted-foreground">
                        Disabled sources are hidden from zone selection
                      </p>
                    </div>
                    <Switch
                      checked={sheetForm.enabled === 1}
                      onCheckedChange={(checked) => {
                        const enabled = checked ? 1 : 0;
                        updateForm({
                          enabled,
                          ...(enabled === 0 ? { isAirplay: false } : {}),
                        });
                      }}
                    />
                  </div>
                </SheetCard>
              </div>

              <div className="space-y-3">
                <SectionHeader title="Audio" />
                <SheetCard className="space-y-4">
                  <div>
                    <div className="mb-2 flex items-center justify-between gap-2">
                      <span className="text-sm font-medium text-foreground">Input gain</span>
                      <span className="text-sm text-muted-foreground">{sheetForm.inputGain}</span>
                    </div>
                    <ZoneEditSlider
                      min={0}
                      max={20}
                      value={sheetForm.inputGain}
                      ariaLabel="Source input gain"
                      onChange={(value) => updateForm({ inputGain: value })}
                    />
                  </div>

                  <label className="grid gap-2">
                    <span className="text-sm font-medium text-foreground">Display line</span>
                    <input
                      type="text"
                      className="h-10 rounded-xl border border-border bg-background px-3 text-sm text-foreground"
                      value={sheetForm.displayLine}
                      placeholder="Shown on keypads and displays"
                      onChange={(event) => updateForm({ displayLine: event.target.value })}
                    />
                  </label>
                </SheetCard>
              </div>

              <div className="space-y-3">
                <SectionHeader title="AirPlay" />
                <SheetCard>
                  <div className="flex items-start justify-between gap-4">
                    <div className="flex gap-3">
                      <div className="flex h-10 w-10 shrink-0 items-center justify-center rounded-xl bg-zone-accent/15">
                        <Airplay className="h-5 w-5 text-zone-accent" />
                      </div>
                      <div>
                        <p className="text-sm font-medium text-foreground">AirPlay source</p>
                        <p className="text-xs text-muted-foreground">
                          Only one source can receive AirPlay audio
                        </p>
                        {isCurrentAirplay ? (
                          <p className="mt-2 text-xs text-muted-foreground">
                            Select another source as AirPlay first to turn this off.
                          </p>
                        ) : null}
                        {sheetForm.enabled !== 1 ? (
                          <p className="mt-2 text-xs text-amber-700 dark:text-amber-300">
                            Enable this source before assigning AirPlay.
                          </p>
                        ) : null}
                      </div>
                    </div>
                    <Switch
                      checked={sheetForm.isAirplay || isCurrentAirplay}
                      disabled={airplaySwitchDisabled}
                      onCheckedChange={(checked) => {
                        if (checked) {
                          updateForm({ isAirplay: true });
                        }
                      }}
                    />
                  </div>
                </SheetCard>
              </div>
            </div>
          </div>
        </div>
      </div>
    </>,
    document.body
  );
}
