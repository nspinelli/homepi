import { AudioCard } from "@/components/audio/audio-card.js";
import { useAudioModule } from "@/hooks/use-audio-module.js";

/**
 * Home dashboard with module cards.
 */
export function HomePage(): React.JSX.Element {
  const { state, nowPlaying, playback } = useAudioModule();
  const snapshot = state.snapshot;
  const connected = snapshot?.hifiConnected ?? false;

  const serviceStatuses = snapshot
    ? [
        { label: "Hi-Fi", status: snapshot.services.hifiSerial },
        { label: "Shairport", status: snapshot.services.shairport },
        { label: "PCM", status: snapshot.services.pcmRouter },
        { label: "nqptp", status: snapshot.services.nqptp },
      ]
    : [];

  return (
    <main className="mx-auto max-w-4xl px-4 py-8">
      <h1 className="mb-6 text-2xl font-semibold text-foreground">Dashboard</h1>
      <div className="grid gap-4 md:grid-cols-2">
        <AudioCard
          name="Home Audio"
          isConnected={connected}
          currentTrack={nowPlaying?.track}
          artist={nowPlaying?.artist}
          album={nowPlaying?.album}
          source={nowPlaying?.source}
          coverUrl={playback?.coverUrl}
          serviceStatuses={nowPlaying ? undefined : serviceStatuses}
        />
      </div>
    </main>
  );
}
