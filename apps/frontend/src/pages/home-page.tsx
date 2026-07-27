import { AudioCard } from "@/components/audio/audio-card.js";
import { AudioCardSkeleton } from "@/components/audio/audio-card-skeleton.js";
import { SensorsCard } from "@/components/sensors/sensors-card.js";
import { SensorsCardSkeleton } from "@/components/sensors/sensors-card-skeleton.js";
import { useAudioModule } from "@/hooks/audio-module-provider.js";
import { useSensorsModule } from "@/hooks/sensors-module-provider.js";
import { deriveAudioConnectionLevelFromSnapshot } from "@/lib/derive-audio-connection-level.js";
import { deriveSensorsConnectionLevelFromSnapshot } from "@/lib/derive-sensors-connection-level.js";

/**
 * Home dashboard with module cards.
 */
export function HomePage(): React.JSX.Element {
  const { state: audioState } = useAudioModule();
  const { state: sensorsState } = useSensorsModule();
  const audioSnapshot = audioState.snapshot;
  const sensorsSnapshot = sensorsState.snapshot;
  const openCount = sensorsSnapshot?.sensors.filter((s) => s.contactState === "open").length;

  return (
    <main className="mx-auto max-w-4xl px-4 py-8">
      <h1 className="mb-6 text-2xl font-semibold text-foreground">Dashboard</h1>
      <div className="grid gap-4 md:grid-cols-2">
        {audioState.loading ? (
          <AudioCardSkeleton />
        ) : (
          <AudioCard
            name="Home Audio"
            connectionLevel={deriveAudioConnectionLevelFromSnapshot(audioSnapshot, {
              servicesHydrated: audioState.snapshotHydrated,
            })}
          />
        )}
        {sensorsState.loading ? (
          <SensorsCardSkeleton />
        ) : (
          <SensorsCard
            name="Contact Sensors"
            connectionLevel={deriveSensorsConnectionLevelFromSnapshot(sensorsSnapshot, {
              servicesHydrated: sensorsState.snapshotHydrated,
            })}
            openCount={openCount}
          />
        )}
      </div>
    </main>
  );
}
