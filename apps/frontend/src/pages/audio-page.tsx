import { useMemo, useState } from "react";
import { Eye, EyeOff } from "lucide-react";

import { isSourceEnabled } from "@/lib/is-source-enabled.js";
import { isZoneEnabled } from "@/lib/is-zone-enabled.js";
import { zoneCardVolume } from "@/lib/zone-card-volume.js";

import { AudioConfigurationCard } from "@/components/audio-configuration-card.js";
import { AudioControllerCard } from "@/components/audio/audio-controller-card.js";
import {
  AudioListSectionSkeleton,
  AudioZonesGridSkeleton,
} from "@/components/audio/audio-page-skeletons.js";
import { AUDIO_SECTION_TABS } from "@/components/audio/audio-section-tabs.js";
import { AudioBottomNav } from "@/components/audio/audio-bottom-nav.js";
import { SourceCard } from "@/components/audio/source-card.js";
import { SourceEditSheet } from "@/components/audio/source-edit-sheet.js";
import { ZoneCard } from "@/components/audio/zone-card.js";
import { ZoneEditSheet } from "@/components/audio/zone-edit-sheet.js";
import { ModulePageHeader } from "@/components/module-page-header.js";
import { Button } from "@/components/ui/button.js";
import { Tabs, TabsContent } from "@/components/ui/tabs.js";
import {
  getShairportSettingsForZone,
  getZoneActivityPriority,
} from "@/hooks/use-audio-module.js";
import { useAudioModule } from "@/hooks/audio-module-provider.js";
import type { HifiSource, HifiZone } from "@/types/audio-types.js";

/** Hi-Fi controller artwork used across Home Audio UI. */
const AUDIO_MODULE_ICON = "/audio-controller.png";

/**
 * Sort priority for source cards: AirPlay first, then enabled, then disabled.
 * @param source - Hi-Fi source row.
 * @returns Lower values sort first.
 */
function getSourceSortPriority(source: HifiSource): number {
  if (source.isAirplay === 1) {
    return 0;
  }
  if (isSourceEnabled(source)) {
    return 1;
  }
  return 2;
}

/**
 * Resolve the visible label for an audio section tab value.
 * @param tabValue - Radix tab value.
 * @returns Human-readable section title.
 */
function audioSectionTitle(tabValue: string): string {
  return AUDIO_SECTION_TABS.find((tab) => tab.value === tabValue)?.label ?? tabValue;
}

/**
 * Audio module detail page with zones, sources, paging, and settings tabs.
 */
export function AudioPage(): React.JSX.Element {
  const {
    state,
    saveZoneSettings,
    saveSourceSettings,
    toggleZonePower,
    setZoneVolume,
    isZoneStreamedTo,
    isZoneSendingAudio,
  } = useAudioModule();
  const [activeTab, setActiveTab] = useState("zones");
  const [showDisabled, setShowDisabled] = useState(false);
  const [showDisabledSources, setShowDisabledSources] = useState(false);
  const [editZoneNumber, setEditZoneNumber] = useState<number | null>(null);
  const [editSourceNumber, setEditSourceNumber] = useState<number | null>(null);

  const snapshot = state.snapshot;
  const zones = snapshot?.zones ?? [];
  const sources = snapshot?.sources ?? [];
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

  const sortedSources = useMemo(() => {
    const visible = showDisabledSources
      ? sources
      : sources.filter((source) => isSourceEnabled(source));

    return [...visible].sort((sourceA, sourceB) => {
      const priorityA = getSourceSortPriority(sourceA);
      const priorityB = getSourceSortPriority(sourceB);
      if (priorityA !== priorityB) {
        return priorityA - priorityB;
      }
      return sourceA.sourceNumber - sourceB.sourceNumber;
    });
  }, [sources, showDisabledSources]);

  const editZone: HifiZone | null =
    editZoneNumber !== null
      ? (zones.find((z) => z.zoneNumber === editZoneNumber) ?? null)
      : null;

  const editSource: HifiSource | null =
    editSourceNumber !== null
      ? (sources.find((s) => s.sourceNumber === editSourceNumber) ?? null)
      : null;

  const editShairport = editZone
    ? getShairportSettingsForZone(snapshot?.shairportZoneSettings ?? [], editZone.zoneNumber)
    : null;

  const isInitialLoad = state.loading && snapshot === null;

  const headerSubtitle = useMemo((): string => {
    switch (activeTab) {
      case "zones":
        return isInitialLoad
          ? "Loading zones…"
          : `${sortedZones.length} of ${zones.length} zones shown`;
      case "sources":
        return isInitialLoad
          ? "Loading sources…"
          : `${sortedSources.length} of ${sources.length} sources shown`;
      case "paging":
        return isInitialLoad
          ? "Loading paging…"
          : `Controller page active: ${snapshot?.controller.pageActive === 1 ? "Yes" : "No"}`;
      case "settings":
        return isInitialLoad ? "Loading settings…" : "Audio configuration and controller";
      default:
        return "";
    }
  }, [
    activeTab,
    isInitialLoad,
    sortedZones.length,
    zones.length,
    sortedSources.length,
    sources.length,
    snapshot?.controller.pageActive,
  ]);

  const headerActions =
    activeTab === "zones" ? (
      <Button
        type="button"
        variant="ghost"
        size="icon"
        aria-label={showDisabled ? "Hide disabled zones" : "Show disabled zones"}
        aria-pressed={showDisabled}
        disabled={isInitialLoad}
        onClick={() => setShowDisabled((value) => !value)}
      >
        {showDisabled ? (
          <Eye className="h-5 w-5 text-muted-foreground" />
        ) : (
          <EyeOff className="h-5 w-5 text-muted-foreground" />
        )}
      </Button>
    ) : activeTab === "sources" ? (
      <Button
        type="button"
        variant="ghost"
        size="icon"
        aria-label={showDisabledSources ? "Hide disabled sources" : "Show disabled sources"}
        aria-pressed={showDisabledSources}
        disabled={isInitialLoad}
        onClick={() => setShowDisabledSources((value) => !value)}
      >
        {showDisabledSources ? (
          <Eye className="h-5 w-5 text-muted-foreground" />
        ) : (
          <EyeOff className="h-5 w-5 text-muted-foreground" />
        )}
      </Button>
    ) : null;

  return (
    <>
    <main className="mx-auto max-w-4xl overflow-x-hidden px-4 py-8">
      <Tabs value={activeTab} onValueChange={setActiveTab} className="w-full">
        <div className="pb-28">
          <ModulePageHeader
            iconSrc={AUDIO_MODULE_ICON}
            title={audioSectionTitle(activeTab)}
            subtitle={headerSubtitle}
            actions={headerActions}
          />
          {state.error ? (
            <p className="-mt-2 mb-6 text-sm text-destructive" role="alert">
              {state.error}
            </p>
          ) : null}

          <TabsContent value="zones">
          {isInitialLoad ? (
            <AudioZonesGridSkeleton count={8} />
          ) : (
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
          )}
        </TabsContent>

        <TabsContent value="sources">
          {isInitialLoad ? (
            <AudioZonesGridSkeleton count={8} />
          ) : (
          <div className="grid min-w-0 grid-cols-1 gap-6 md:grid-cols-2">
            {sortedSources.map((source) => (
              <SourceCard
                key={source.sourceNumber}
                id={source.sourceNumber}
                name={source.name ?? `Source ${source.sourceNumber}`}
                isEnabled={isSourceEnabled(source)}
                isAirplay={source.isAirplay === 1}
                inputGain={source.inputGain}
                displayLine={source.displayLine}
                onEdit={setEditSourceNumber}
              />
            ))}
          </div>
          )}
        </TabsContent>

        <TabsContent value="paging">
          {isInitialLoad ? (
            <AudioListSectionSkeleton titleWidth="w-24" rows={2} />
          ) : (
          <div className="rounded-lg border border-border bg-card p-6">
            <p className="text-sm text-muted-foreground">
              Assign the paging USB DAC under Settings → Audio Configuration.
            </p>
          </div>
          )}
        </TabsContent>

        <TabsContent value="settings">
          {isInitialLoad ? (
            <div className="grid gap-4">
              <AudioListSectionSkeleton titleWidth="w-44" rows={3} />
              <AudioListSectionSkeleton titleWidth="w-28" rows={2} />
            </div>
          ) : (
          <div className="grid gap-4">
            <AudioConfigurationCard />
            <AudioControllerCard />
          </div>
          )}
        </TabsContent>
        </div>

        <AudioBottomNav />
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

    <SourceEditSheet
      open={editSourceNumber !== null}
      onOpenChange={(open) => {
        if (!open) {
          setEditSourceNumber(null);
        }
      }}
      source={editSource}
      saving={state.savingSource === editSourceNumber}
      onSave={saveSourceSettings}
    />
    </>
  );
}
