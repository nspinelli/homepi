import { useEffect, useState } from "react";

import {
  buildZoneSettingsPatch,
  zoneToFormState,
  type ZoneSettingsFormState,
} from "@/components/audio/zone-settings-patch.js";
import { Button } from "@/components/ui/button.js";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogTitle,
} from "@/components/ui/dialog.js";
import { Input } from "@/components/ui/input.js";
import { Label } from "@/components/ui/label.js";
import type { HifiZone, ShairportZoneSettings, ZoneSettingsPatch } from "@/types/audio-types.js";

/**
 * Props for the zone settings editor dialog.
 */
export interface ZoneSettingsDialogProps {
  /** Whether the dialog is open. */
  open: boolean;
  /** Close handler. */
  onOpenChange: (open: boolean) => void;
  /** Zone row from snapshot. */
  zone: HifiZone | null;
  /** Shairport settings for this zone. */
  shairport: ShairportZoneSettings | null;
  /** Whether a save is in progress. */
  saving: boolean;
  /** Persists zone settings. */
  onSave: (zoneNumber: number, patch: ZoneSettingsPatch) => Promise<void>;
}

/**
 * Legacy centered dialog for controller and Shairport zone settings.
 * Prefer {@link ZoneEditSheet} for the primary mobile experience.
 */
export function ZoneSettingsDialog({
  open,
  onOpenChange,
  zone,
  shairport,
  saving,
  onSave,
}: ZoneSettingsDialogProps): React.JSX.Element | null {
  const [form, setForm] = useState<ZoneSettingsFormState | null>(null);

  useEffect(() => {
    if (!zone) {
      return;
    }
    setForm(zoneToFormState(zone, shairport));
  }, [zone, shairport, open]);

  if (!zone || !form) {
    return null;
  }

  const handleSave = async (): Promise<void> => {
    const patch = buildZoneSettingsPatch(zone, shairport, form);
    if (!patch) {
      onOpenChange(false);
      return;
    }

    await onSave(zone.zoneNumber, patch);
    onOpenChange(false);
  };

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="sm:max-w-md">
        <DialogTitle>Zone {zone.zoneNumber}</DialogTitle>
        <DialogDescription>Controller and AirPlay (Shairport) settings</DialogDescription>

        <div className="grid gap-4 py-2">
          <fieldset className="grid gap-3">
            <legend className="text-sm font-medium text-foreground">Controller</legend>
            <label className="grid gap-1.5">
              <Label htmlFor="zone-name">Name</Label>
              <Input
                id="zone-name"
                value={form.name}
                onChange={(e) => setForm({ ...form, name: e.target.value })}
              />
            </label>
            <label className="grid gap-1.5">
              <Label htmlFor="zone-enabled">Enabled (0/1)</Label>
              <Input
                id="zone-enabled"
                type="number"
                min={0}
                max={1}
                value={form.enabled}
                onChange={(e) => setForm({ ...form, enabled: Number(e.target.value) })}
              />
            </label>
            <div className="grid grid-cols-2 gap-3">
              <label className="grid gap-1.5">
                <Label htmlFor="zone-treble">Treble</Label>
                <Input
                  id="zone-treble"
                  type="number"
                  value={form.treble}
                  onChange={(e) => setForm({ ...form, treble: Number(e.target.value) })}
                />
              </label>
              <label className="grid gap-1.5">
                <Label htmlFor="zone-bass">Bass</Label>
                <Input
                  id="zone-bass"
                  type="number"
                  value={form.bass}
                  onChange={(e) => setForm({ ...form, bass: Number(e.target.value) })}
                />
              </label>
            </div>
            <div className="grid grid-cols-2 gap-3">
              <label className="grid gap-1.5">
                <Label htmlFor="zone-balance">Balance</Label>
                <Input
                  id="zone-balance"
                  type="number"
                  value={form.balance}
                  onChange={(e) => setForm({ ...form, balance: Number(e.target.value) })}
                />
              </label>
              <label className="grid gap-1.5">
                <Label htmlFor="zone-loudness">Loudness</Label>
                <Input
                  id="zone-loudness"
                  type="number"
                  value={form.loudness}
                  onChange={(e) => setForm({ ...form, loudness: Number(e.target.value) })}
                />
              </label>
            </div>
            <div className="grid grid-cols-2 gap-3">
              <label className="grid gap-1.5">
                <Label htmlFor="zone-inivol">Initial volume</Label>
                <Input
                  id="zone-inivol"
                  type="number"
                  min={0}
                  max={100}
                  value={form.initialVolume}
                  onChange={(e) => setForm({ ...form, initialVolume: Number(e.target.value) })}
                />
              </label>
              <label className="grid gap-1.5">
                <Label htmlFor="zone-pgvol">Page volume</Label>
                <Input
                  id="zone-pgvol"
                  type="number"
                  min={0}
                  max={100}
                  value={form.pageVolume}
                  onChange={(e) => setForm({ ...form, pageVolume: Number(e.target.value) })}
                />
              </label>
            </div>
            <label className="grid gap-1.5">
              <Label htmlFor="zone-group">Group</Label>
              <Input
                id="zone-group"
                type="number"
                min={0}
                max={8}
                value={form.groupNumber}
                onChange={(e) => setForm({ ...form, groupNumber: Number(e.target.value) })}
              />
            </label>
          </fieldset>

          <fieldset className="grid gap-3 border-t border-border pt-4">
            <legend className="text-sm font-medium text-foreground">Shairport Sync</legend>
            <label className="grid gap-1.5">
              <Label htmlFor="sp-profile">Volume profile</Label>
              <select
                id="sp-profile"
                className="h-10 w-full rounded-md border border-border bg-background px-3 text-sm"
                value={form.volumeProfile}
                onChange={(e) => setForm({ ...form, volumeProfile: e.target.value })}
              >
                <option value="standard">standard</option>
                <option value="flat">flat</option>
                <option value="dasl_tapered">dasl_tapered</option>
              </select>
            </label>
            <label className="grid gap-1.5">
              <Label htmlFor="sp-active-timeout">Active state timeout (s)</Label>
              <Input
                id="sp-active-timeout"
                type="number"
                step={0.1}
                value={form.activeTimeout}
                onChange={(e) => setForm({ ...form, activeTimeout: Number(e.target.value) })}
              />
            </label>
            <label className="grid gap-1.5">
              <Label htmlFor="sp-session-timeout">Session timeout (s)</Label>
              <Input
                id="sp-session-timeout"
                type="number"
                value={form.sessionTimeout}
                onChange={(e) => setForm({ ...form, sessionTimeout: Number(e.target.value) })}
              />
            </label>
            <label className="grid gap-1.5">
              <Label htmlFor="sp-log">Log verbosity (0-3)</Label>
              <Input
                id="sp-log"
                type="number"
                min={0}
                max={3}
                value={form.logVerbosity}
                onChange={(e) => setForm({ ...form, logVerbosity: Number(e.target.value) })}
              />
            </label>
          </fieldset>
        </div>

        <div className="flex justify-end gap-2">
          <Button variant="outline" onClick={() => onOpenChange(false)} disabled={saving}>
            Cancel
          </Button>
          <Button onClick={() => void handleSave()} disabled={saving}>
            {saving ? "Saving…" : "Save"}
          </Button>
        </div>
      </DialogContent>
    </Dialog>
  );
}
