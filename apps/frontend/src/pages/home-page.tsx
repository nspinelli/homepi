import { AudioCard } from "@/components/audio/audio-card.js";
import { AudioCardSkeleton } from "@/components/audio/audio-card-skeleton.js";
import { useAudioModule } from "@/hooks/audio-module-provider.js";
import { deriveAudioConnectionLevel } from "@/lib/derive-audio-connection-level.js";

/**
 * Home dashboard with module cards.
 */
export function HomePage(): React.JSX.Element {
  const { state } = useAudioModule();
  const snapshot = state.snapshot;
  const connectionLevel = deriveAudioConnectionLevel(
    snapshot?.hifiConnected ?? false,
    snapshot?.services
  );

  return (
    <main className="mx-auto max-w-4xl px-4 py-8">
      <h1 className="mb-6 text-2xl font-semibold text-foreground">Dashboard</h1>
      <div className="grid gap-4 md:grid-cols-2">
        {state.loading && snapshot === null ? (
          <AudioCardSkeleton />
        ) : (
          <AudioCard name="Home Audio" connectionLevel={connectionLevel} />
        )}
      </div>
    </main>
  );
}
