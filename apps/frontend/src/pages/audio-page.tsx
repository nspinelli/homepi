import { useMemo, useState } from "react";
import { ArrowLeft, Eye, EyeOff } from "lucide-react";

import { isZoneEnabled } from "@/lib/is-zone-enabled.js";
import { zoneInitialVolume } from "@/lib/zone-initial-volume.js";
import { Link, useNavigate } from "react-router-dom";

import { AudioConfigurationCard } from "@/components/audio-configuration-card.js";
import { ZoneCard } from "@/components/audio/zone-card.js";
import { ZoneEditSheet } from "@/components/audio/zone-edit-sheet.js";
import { Button } from "@/components/ui/button.js";
import { AudioPlayerBar } from "@/components/audio/audio-player-bar.js";
import { AudioSectionTabsList } from "@/components/audio/audio-section-tabs.js";
import { Tabs, TabsContent } from "@/components/ui/tabs.js";
import {
  getShairportSettingsForZone,
  getZoneActivityPriority,
  useAudioModule,
} from "@/hooks/use-audio-module.js";
import type { HifiZone } from "@/types/audio-types.js";

/**
 * Volume shown on the zone card slider (live when on or streamed, initial when off).
 * @param zone - Hi-Fi zone row.
 * @param isStreamedTo - Whether PCM router has an active AirPlay route to this zone.
 * @returns Volume 0–100 for the slider display.
 */
function zoneCardVolume(zone: HifiZone, isStreamedTo: boolean): number {
  const initialVolume = zoneInitialVolume(zone);
  if ((zone.power ?? 0) === 1 || isStreamedTo) {
    return zone.volume ?? initialVolume;
  }
  return initialVolume;
}

/**
 * Audio module detail page with zones, sources, groups, paging, and settings tabs.
 */
export function AudioPage(): React.JSX.Element {
  const navigate = useNavigate();
  const {
    state,
    saveZoneSettings,
    toggleZonePower,
    setZoneVolume,
    isZoneStreamedTo,
    isZoneSendingAudio,
    playback,
    sendPlaybackCommand,
    setPlaybackVolume,
  } = useAudioModule();
  const [showDisabled, setShowDisabled] = useState(false);
  const [editZoneNumber, setEditZoneNumber] = useState<number | null>(null);

  const snapshot = state.snapshot;
  const zones = snapshot?.zones ?? [];
  const sortedZones = useMemo(() => {
    const visible = showDisabled ? zones : zones.filter((zone) => isZoneEnabled(zone));

    return [...visible].sort((zoneA, zoneB) => {
      const priorityA = getZoneActivityPriority(
        zoneA,
        isZoneSendingAudio(zoneA.zoneNumber),
        isZoneStreamedTo(zoneA.zoneNumber)
      );
      const priorityB = getZoneActivityPriority(
        zoneB,
        isZoneSendingAudio(zoneB.zoneNumber),
        isZoneStreamedTo(zoneB.zoneNumber)
      );

      if (priorityA !== priorityB) {
        return priorityA - priorityB;
      }

      return zoneA.zoneNumber - zoneB.zoneNumber;
    });
  }, [zones, showDisabled, isZoneSendingAudio, isZoneStreamedTo]);

  const editZone: HifiZone | null =
    editZoneNumber !== null
      ? (zones.find((z) => z.zoneNumber === editZoneNumber) ?? null)
      : null;

  const editShairport = editZone
    ? getShairportSettingsForZone(snapshot?.shairportZoneSettings ?? [], editZone.zoneNumber)
    : null;

  const airplaySource = useMemo(
    () => snapshot?.sources.find((s) => s.isAirplay === 1)?.sourceNumber ?? null,
    [snapshot?.sources]
  );

  return (
    <>
    <main className="mx-auto max-w-5xl overflow-x-hidden px-4 py-8">
      <div className="mb-6">
        <Button
          variant="ghost"
          size="sm"
          className="mb-4 gap-2"
          onClick={() => navigate(-1)}
        >
          <ArrowLeft className="h-4 w-4" />
          Back
        </Button>
        <h1 className="text-2xl font-semibold text-foreground">Home Audio</h1>
        <p className="mt-1 text-muted-foreground">Manage your audio zones and settings</p>
        {state.error ? (
          <p className="mt-2 text-sm text-destructive" role="alert">
            {state.error}
          </p>
        ) : null}
        {state.loading ? (
          <p className="mt-2 text-sm text-muted-foreground">Loading audio state…</p>
        ) : null}
      </div>

      <Tabs defaultValue="zones" className="w-full">
        {playback?.visible ? (
          <AudioPlayerBar
            playback={playback}
            onCommand={sendPlaybackCommand}
            onVolumeChange={setPlaybackVolume}
          />
        ) : null}
        <AudioSectionTabsList />

        <TabsContent value="zones">
          <div className="mb-6 flex items-center justify-between gap-4">
            <div className="min-w-0">
              <h2 className="text-lg font-medium text-foreground">Audio Zones</h2>
              <p className="mt-1 text-sm text-muted-foreground">
                {sortedZones.length} of {zones.length} zones shown
              </p>
            </div>
            <Button
              type="button"
              variant="ghost"
              size="icon"
              className="shrink-0 self-center"
              aria-label={showDisabled ? "Hide disabled zones" : "Show disabled zones"}
              aria-pressed={showDisabled}
              onClick={() => setShowDisabled((value) => !value)}
            >
              {showDisabled ? (
                <Eye className="h-5 w-5 text-muted-foreground" />
              ) : (
                <EyeOff className="h-5 w-5 text-muted-foreground" />
              )}
            </Button>
          </div>
          <div className="grid min-w-0 grid-cols-1 gap-6 md:grid-cols-2">
            {sortedZones.map((zone) => {
              const streamedTo = isZoneStreamedTo(zone.zoneNumber);
              return (
              <ZoneCard
                key={zone.zoneNumber}
                id={zone.zoneNumber}
                name={zone.name ?? `Zone ${zone.zoneNumber}`}
                isEnabled={isZoneEnabled(zone)}
                isOn={(zone.power ?? 0) === 1}
                volume={zoneCardVolume(zone, streamedTo)}
                sourceNumber={zone.source}
                isStreamedTo={streamedTo}
                isSendingAudio={isZoneSendingAudio(zone.zoneNumber)}
                isTogglingPower={state.togglingPowerZone === zone.zoneNumber}
                onTogglePower={toggleZonePower}
                onVolumeChange={setZoneVolume}
                onEdit={setEditZoneNumber}
              />
            );
            })}
          </div>
        </TabsContent>

        <TabsContent value="sources">
          <div className="rounded-lg border border-border bg-card p-6">
            <h2 className="text-lg font-medium text-foreground">Sources</h2>
            <p className="mt-1 text-sm text-muted-foreground">
              AirPlay source slot: {airplaySource ?? "not configured"}
            </p>
            <ul className="mt-4 grid gap-2">
              {(snapshot?.sources ?? []).map((source) => (
                <li
                  key={source.sourceNumber}
                  className="flex items-center justify-between rounded-md border border-border px-3 py-2 text-sm"
                >
                  <span>
                    {source.sourceNumber}. {source.name ?? "Unnamed"}
                    {source.isAirplay === 1 ? " (AirPlay)" : ""}
                  </span>
                  <span className="text-muted-foreground">
                    {source.enabled === 1 ? "enabled" : "disabled"}
                  </span>
                </li>
              ))}
            </ul>
            <p className="mt-4 text-sm text-muted-foreground">
              Source editing via REST will be expanded in a follow-up; use Settings → sync or
              controller UI for now.
            </p>
          </div>
        </TabsContent>

        <TabsContent value="groups">
          <div className="rounded-lg border border-border bg-card p-6">
            <h2 className="text-lg font-medium text-foreground">Groups</h2>
            <ul className="mt-4 grid gap-2">
              {(snapshot?.groups ?? []).map((group) => (
                <li
                  key={group.groupNumber}
                  className="rounded-md border border-border px-3 py-2 text-sm"
                >
                  {group.groupNumber}. {group.name ?? "Unnamed"} (type {group.type ?? 0})
                </li>
              ))}
            </ul>
          </div>
        </TabsContent>

        <TabsContent value="paging">
          <div className="rounded-lg border border-border bg-card p-6">
            <h2 className="text-lg font-medium text-foreground">Paging</h2>
            <p className="mt-1 text-sm text-muted-foreground">
              Controller page active:{" "}
              {snapshot?.controller.pageActive === 1 ? "Yes" : "No"}
            </p>
            <p className="mt-4 text-sm text-muted-foreground">
              Assign the paging USB DAC under Settings → Audio Configuration.
            </p>
          </div>
        </TabsContent>

        <TabsContent value="settings">
          <div className="grid gap-4">
            <AudioConfigurationCard />
            <div className="rounded-lg border border-border bg-card p-6">
              <h2 className="font-medium text-foreground">Controller</h2>
              <p className="mt-1 text-sm text-muted-foreground">
                Trigger a full Hi-Fi2 sync from the controller.
              </p>
              <Button className="mt-4" variant="outline" asChild>
                <Link to="/status">View system status</Link>
              </Button>
            </div>
          </div>
        </TabsContent>
      </Tabs>
    </main>

    <ZoneEditSheet
      open={editZoneNumber !== null}
      onOpenChange={(open) => {
        if (!open) {
          setEditZoneNumber(null);
        }
      }}
      zone={editZone}
      shairport={editShairport ?? null}
      sources={snapshot?.sources ?? []}
      saving={state.savingZone === editZoneNumber}
      onSave={saveZoneSettings}
    />
    </>
  );
}
