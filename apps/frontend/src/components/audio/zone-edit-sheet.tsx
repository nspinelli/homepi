import {
  ArrowLeftRight,
  ChefHat,
  Clock,
  Sparkles,
  Volume2,
  Waves,
  X,
} from "lucide-react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { createPortal } from "react-dom";

import { ZoneEditSlider } from "@/components/audio/zone-edit-slider.js";
import {
  buildZoneSettingsPatch,
  hasZoneSettingsChanges,
  zoneSettingsSnapshotKey,
  zoneToFormState,
  type ZoneSettingsFormState,
} from "@/components/audio/zone-settings-patch.js";
import { Button } from "@/components/ui/button.js";
import { Switch } from "@/components/ui/switch.js";
import { cn } from "@/lib/utils.js";
import type { HifiSource, HifiZone, ShairportZoneSettings, ZoneSettingsPatch } from "@/types/audio-types.js";

/**
 * Props for the iOS-style zone edit bottom sheet.
 */
export interface ZoneEditSheetProps {
  /** Whether the sheet is open. */
  open: boolean;
  /** Open state change handler. */
  onOpenChange: (open: boolean) => void;
  /** Zone row from snapshot. */
  zone: HifiZone | null;
  /** Shairport settings for this zone. */
  shairport: ShairportZoneSettings | null;
  /** Available Hi-Fi sources for labels. */
  sources: HifiSource[];
  /** Whether a save is in progress. */
  saving: boolean;
  /** Persists zone settings. Rejects when the save request fails. */
  onSave: (zoneNumber: number, patch: ZoneSettingsPatch) => Promise<void>;
}

/**
 * Formats a signed integer for bass/treble/balance displays.
 * @param value - Numeric tuning value.
 * @returns Display string with sign.
 */
function formatSignedValue(value: number): string {
  if (value > 0) {
    return `+${value}`;
  }
  return String(value);
}

/**
 * Resolves a human-readable source label.
 * @param sourceNumber - Source slot number.
 * @param sources - Source rows from snapshot.
 * @returns Label like "Source 2 • Analog Input".
 */
function formatSourceLabel(sourceNumber: number | undefined, sources: HifiSource[]): string {
  if (sourceNumber === undefined) {
    return "No source";
  }
  const source = sources.find((row) => row.sourceNumber === sourceNumber);
  const name = source?.name ?? source?.displayLine ?? "Unnamed";
  return `Source ${sourceNumber} • ${name}`;
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
 * iOS-style bottom sheet for editing Hi-Fi2 zone settings.
 */
export function ZoneEditSheet({
  open,
  onOpenChange,
  zone,
  shairport,
  sources,
  saving,
  onSave,
}: ZoneEditSheetProps): React.JSX.Element | null {
  const [form, setForm] = useState<ZoneSettingsFormState | null>(() =>
    zone ? zoneToFormState(zone, shairport) : null
  );
  const [previewVolume, setPreviewVolume] = useState(() => zone?.volume ?? 0);
  const zoneSnapshotKey = useMemo(
    () => (zone ? zoneSettingsSnapshotKey(zone, shairport) : ""),
    [zone, shairport]
  );

  const openedZoneRef = useRef<number | null>(null);

  useEffect(() => {
    if (!open || !zone) {
      if (!open) {
        openedZoneRef.current = null;
      }
      return;
    }

    const isNewSession = openedZoneRef.current !== zone.zoneNumber;
    openedZoneRef.current = zone.zoneNumber;

    if (isNewSession) {
      setForm(zoneToFormState(zone, shairport));
      setPreviewVolume(zone.volume ?? 0);
      return;
    }

    setForm((current) => {
      if (current && hasZoneSettingsChanges(zone, shairport, current)) {
        return current;
      }
      return zoneToFormState(zone, shairport);
    });
    setPreviewVolume((currentPreview) => {
      return zone.volume ?? currentPreview;
    });
  }, [open, zone, shairport, zoneSnapshotKey]);

  const updateForm = useCallback(
    (partial: Partial<ZoneSettingsFormState>) => {
      setForm((current) => {
        const base = current ?? (zone ? zoneToFormState(zone, shairport) : null);
        if (!base) {
          return current;
        }
        return { ...base, ...partial };
      });
    },
    [zone, shairport]
  );

  const unsaved = useMemo(() => {
    if (!zone) {
      return false;
    }
    const activeForm = form ?? zoneToFormState(zone, shairport);
    return hasZoneSettingsChanges(zone, shairport, activeForm);
  }, [zone, shairport, form]);

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
    if (!zone || saving) {
      return;
    }
    const activeForm = form ?? zoneToFormState(zone, shairport);
    const patch = buildZoneSettingsPatch(zone, shairport, activeForm);
    if (!patch) {
      handleClose();
      return;
    }
    try {
      await onSave(zone.zoneNumber, patch);
      handleClose();
    } catch {
      /* Toast shown by saveZoneSettings; keep sheet open for corrections. */
    }
  }, [zone, shairport, form, saving, onSave, handleClose]);

  const sourceLabel = zone ? formatSourceLabel(zone.source, sources) : "";

  if (!open || !zone) {
    return null;
  }

  const sheetForm = form ?? (zone ? zoneToFormState(zone, shairport) : null);

  return createPortal(
    <>
      <button
        type="button"
        className="fixed inset-0 z-[200] bg-black/40 backdrop-blur-[2px]"
        aria-label="Close edit zone"
        onClick={handleClose}
      />

      <div
        role="dialog"
        aria-modal="true"
        aria-labelledby="zone-edit-sheet-title"
        className={cn(
          "fixed inset-x-0 bottom-0 z-[210] mx-auto flex w-full max-w-lg flex-col overflow-hidden",
          "max-h-[min(92vh,900px)] min-h-[50vh]",
          "rounded-t-[1.75rem] border border-border/60 bg-card text-card-foreground shadow-2xl"
        )}
        onClick={(event) => event.stopPropagation()}
      >
        {sheetForm ? (
          <>
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
                  <h2 id="zone-edit-sheet-title" className="text-base font-semibold text-foreground">
                    Edit Zone
                  </h2>
                  <span className="mt-1 inline-block rounded-full bg-zone-accent/15 px-2.5 py-0.5 text-xs font-medium text-zone-accent">
                    Zone {zone.zoneNumber}
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
                <div className="space-y-6 px-4 pt-2 pb-4">
              <SheetCard>
                <div className="flex gap-4">
                  <div className="flex h-14 w-14 shrink-0 items-center justify-center rounded-2xl bg-zone-accent/15">
                    <ChefHat className="h-7 w-7 text-zone-accent" />
                  </div>
                  <div className="min-w-0 flex-1">
                    <p className="text-sm font-medium text-foreground">Preview</p>
                    <p className="text-xs text-muted-foreground">This is how your zone will appear</p>
                  </div>
                </div>

                <div className="mt-4">
                  <input
                    type="text"
                    value={sheetForm.name}
                    onChange={(event) => updateForm({ name: event.target.value })}
                    className="w-full border-0 bg-transparent p-0 text-2xl font-bold text-foreground outline-none focus:ring-0"
                    aria-label="Zone name"
                  />
                  <p className="mt-0.5 text-sm text-muted-foreground">{sourceLabel}</p>
                </div>

                <div className="mt-4 flex items-center justify-between gap-3">
                  <label className="flex items-center gap-2 text-sm text-muted-foreground">
                    <span>Enabled</span>
                  </label>
                  <Switch
                    checked={sheetForm.enabled === 1}
                    onCheckedChange={(checked) => updateForm({ enabled: checked ? 1 : 0 })}
                    className="data-[state=checked]:bg-zone-accent"
                  />
                </div>

                <div className="mt-4 flex items-center gap-3">
                  <Volume2 className="h-4 w-4 shrink-0 text-muted-foreground" />
                  <ZoneEditSlider
                    min={0}
                    max={100}
                    value={previewVolume}
                    onChange={setPreviewVolume}
                    ariaLabel="Preview volume"
                  />
                  <span className="w-10 shrink-0 text-right text-sm text-muted-foreground">
                    {previewVolume}%
                  </span>
                </div>
              </SheetCard>

              <section className="space-y-3">
                <SectionHeader title="Zone Behavior" />
                <SheetCard className="divide-y divide-border/60 p-0">
                  <div className="flex items-center gap-3 p-4">
                    <div className="flex h-9 w-9 shrink-0 items-center justify-center rounded-xl bg-muted">
                      <Clock className="h-4 w-4 text-muted-foreground" />
                    </div>
                    <div className="min-w-0 flex-1">
                      <p className="text-sm font-medium text-foreground">Enabled</p>
                      <p className="text-xs text-muted-foreground">
                        Disabled zones cannot be controlled
                      </p>
                    </div>
                    <Switch
                      checked={sheetForm.enabled === 1}
                      onCheckedChange={(checked) => updateForm({ enabled: checked ? 1 : 0 })}
                      className="data-[state=checked]:bg-zone-accent"
                    />
                  </div>

                  <div className="space-y-4 p-4">
                    <div className="flex items-center gap-3">
                      <Volume2 className="h-4 w-4 shrink-0 text-muted-foreground" />
                      <div className="min-w-0 flex-1">
                        <p className="text-sm font-medium text-foreground">Initial Volume</p>
                      </div>
                      <span className="w-10 shrink-0 text-right text-sm text-muted-foreground">
                        {sheetForm.initialVolume}%
                      </span>
                    </div>
                    <ZoneEditSlider
                      min={0}
                      max={100}
                      value={sheetForm.initialVolume}
                      onChange={(value) => updateForm({ initialVolume: value })}
                      ariaLabel="Initial volume"
                    />
                  </div>

                  <div className="space-y-4 p-4">
                    <div className="flex items-center gap-3">
                      <Volume2 className="h-4 w-4 shrink-0 text-muted-foreground" />
                      <div className="min-w-0 flex-1">
                        <p className="text-sm font-medium text-foreground">Page Volume</p>
                      </div>
                      <span className="w-10 shrink-0 text-right text-sm text-muted-foreground">
                        {sheetForm.pageVolume}%
                      </span>
                    </div>
                    <ZoneEditSlider
                      min={0}
                      max={100}
                      value={sheetForm.pageVolume}
                      onChange={(value) => updateForm({ pageVolume: value })}
                      ariaLabel="Page volume"
                    />
                  </div>
                </SheetCard>
              </section>

              <section className="space-y-3">
                <SectionHeader title="Audio Tuning" />
                <SheetCard className="space-y-5">
                  <div className="space-y-2">
                    <div className="flex items-center gap-3">
                      <Waves className="h-4 w-4 shrink-0 text-muted-foreground" />
                      <span className="flex-1 text-sm font-medium text-foreground">Bass</span>
                      <span className="min-w-[2.5rem] rounded-lg border border-border bg-muted/50 px-2 py-0.5 text-center text-xs font-medium tabular-nums">
                        {formatSignedValue(sheetForm.bass)}
                      </span>
                    </div>
                    <div className="flex items-center gap-2 text-[10px] text-muted-foreground">
                      <span className="w-6">-10</span>
                      <ZoneEditSlider
                        min={-10}
                        max={10}
                        value={sheetForm.bass}
                        onChange={(value) => updateForm({ bass: value })}
                        ariaLabel="Bass"
                      />
                      <span className="w-6 text-right">+10</span>
                    </div>
                  </div>

                  <div className="space-y-2">
                    <div className="flex items-center gap-3">
                      <Sparkles className="h-4 w-4 shrink-0 text-muted-foreground" />
                      <span className="flex-1 text-sm font-medium text-foreground">Treble</span>
                      <span className="min-w-[2.5rem] rounded-lg border border-border bg-muted/50 px-2 py-0.5 text-center text-xs font-medium tabular-nums">
                        {formatSignedValue(sheetForm.treble)}
                      </span>
                    </div>
                    <div className="flex items-center gap-2 text-[10px] text-muted-foreground">
                      <span className="w-6">-10</span>
                      <ZoneEditSlider
                        min={-10}
                        max={10}
                        value={sheetForm.treble}
                        onChange={(value) => updateForm({ treble: value })}
                        ariaLabel="Treble"
                      />
                      <span className="w-6 text-right">+10</span>
                    </div>
                  </div>

                  <div className="space-y-2">
                    <div className="flex items-center gap-3">
                      <ArrowLeftRight className="h-4 w-4 shrink-0 text-muted-foreground" />
                      <span className="flex-1 text-sm font-medium text-foreground">Balance</span>
                      <span className="min-w-[2.5rem] rounded-lg border border-border bg-muted/50 px-2 py-0.5 text-center text-xs font-medium tabular-nums">
                        {formatSignedValue(sheetForm.balance)}
                      </span>
                    </div>
                    <ZoneEditSlider
                      min={-10}
                      max={10}
                      value={sheetForm.balance}
                      onChange={(value) => updateForm({ balance: value })}
                      ariaLabel="Balance"
                    />
                    <div className="flex justify-between text-[10px] text-muted-foreground">
                      <span>Left</span>
                      <span>Center</span>
                      <span>Right</span>
                    </div>
                  </div>

                  <div className="flex items-center gap-3 border-t border-border/60 pt-4">
                    <Volume2 className="h-4 w-4 shrink-0 text-muted-foreground" />
                    <span className="flex-1 text-sm font-medium text-foreground">Loudness</span>
                    <Switch
                      checked={sheetForm.loudness === 1}
                      onCheckedChange={(checked) => updateForm({ loudness: checked ? 1 : 0 })}
                      className="data-[state=checked]:bg-zone-accent"
                    />
                  </div>
                </SheetCard>
              </section>

              <section className="space-y-3">
                <SectionHeader title="Shairport Sync" />
                <SheetCard className="space-y-4">
                  <label className="grid gap-1.5">
                    <span className="text-sm text-foreground">Volume profile</span>
                    <select
                      className="h-10 w-full rounded-lg border border-border bg-background px-3 text-sm"
                      value={sheetForm.volumeProfile}
                      onChange={(event) => updateForm({ volumeProfile: event.target.value })}
                    >
                      <option value="standard">standard</option>
                      <option value="flat">flat</option>
                      <option value="dasl_tapered">dasl_tapered</option>
                    </select>
                  </label>
                  <label className="grid gap-1.5">
                    <span className="text-sm text-foreground">Active state timeout (s)</span>
                    <input
                      type="number"
                      step={0.1}
                      className="h-10 w-full rounded-lg border border-border bg-background px-3 text-sm"
                      value={sheetForm.activeTimeout}
                      onChange={(event) =>
                        updateForm({ activeTimeout: Number(event.target.value) })
                      }
                    />
                  </label>
                  <label className="grid gap-1.5">
                    <span className="text-sm text-foreground">Session timeout (s)</span>
                    <input
                      type="number"
                      className="h-10 w-full rounded-lg border border-border bg-background px-3 text-sm"
                      value={sheetForm.sessionTimeout}
                      onChange={(event) =>
                        updateForm({ sessionTimeout: Number(event.target.value) })
                      }
                    />
                  </label>
                  <label className="grid gap-1.5">
                    <span className="text-sm text-foreground">Log verbosity (0–3)</span>
                    <input
                      type="number"
                      min={0}
                      max={3}
                      className="h-10 w-full rounded-lg border border-border bg-background px-3 text-sm"
                      value={sheetForm.logVerbosity}
                      onChange={(event) =>
                        updateForm({ logVerbosity: Number(event.target.value) })
                      }
                    />
                  </label>
                </SheetCard>
              </section>
                </div>

                <div
                  className="h-[calc(4.75rem+env(safe-area-inset-bottom))] shrink-0"
                  aria-hidden
                />
              </div>

              <footer className="absolute inset-x-0 bottom-0 z-20 border-t border-border/60 bg-card/70 px-4 py-3 shadow-[0_-4px_24px_rgba(0,0,0,0.06)] backdrop-blur-md supports-[backdrop-filter]:bg-card/60">
                <div className="flex items-center gap-2 pb-[env(safe-area-inset-bottom)]">
              <Button
                type="button"
                variant="secondary"
                className="flex-1 rounded-xl bg-muted text-foreground hover:bg-muted/80"
                disabled={saving}
                onClick={handleClose}
              >
                Cancel
              </Button>

              <div className="flex min-w-0 flex-1 items-center justify-center gap-1.5">
                {unsaved ? (
                  <>
                    <span className="h-2 w-2 shrink-0 rounded-full bg-warning" aria-hidden />
                    <span className="truncate text-xs text-muted-foreground">Unsaved changes</span>
                  </>
                ) : (
                  <span className="truncate text-xs text-muted-foreground">No changes</span>
                )}
              </div>

              <Button
                type="button"
                className="flex-1 rounded-xl bg-zone-accent text-white hover:bg-zone-accent/90"
                disabled={saving || !unsaved}
                onClick={() => void handleSave()}
              >
                {saving ? "Saving…" : "Save Changes"}
              </Button>
                </div>
              </footer>
            </div>
          </>
        ) : null}
      </div>
    </>,
    document.body
  );
}
