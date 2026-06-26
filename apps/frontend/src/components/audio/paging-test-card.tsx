import { useMemo, useState } from "react";
import { CircleAlert, Loader2, Megaphone } from "lucide-react";

import { Button } from "@/components/ui/button.js";
import { Switch } from "@/components/ui/switch.js";
import type { UsePagingModuleResult } from "@/hooks/use-paging-module.js";

/**
 * Props for paging test card.
 */
export interface PagingTestCardProps {
  /** Shared paging module hook result. */
  paging: UsePagingModuleResult;
}

/**
 * Whole-house paging test card with custom message input.
 * @param props - Component props.
 * @returns Paging test card element.
 */
export function PagingTestCard({ paging }: PagingTestCardProps): React.JSX.Element {
  const { state, testDac, testPage } = paging;
  const [message, setMessage] = useState("Testing HomePi paging.");
  const [includeChime, setIncludeChime] = useState(false);
  const [selectedVoiceId, setSelectedVoiceId] = useState("");
  const [actionError, setActionError] = useState<string | null>(null);
  const [awaitingConfirm, setAwaitingConfirm] = useState(false);

  const maxTextLength = state.config?.maxTextLength ?? 4000;
  const pagingEnabled = state.config?.enabled ?? false;
  const isBusy = Boolean(state.pendingActions.testPage);

  const installedVoices = useMemo(
    () => state.voices.filter((voice) => voice.installed),
    [state.voices]
  );

  const defaultVoiceId =
    state.config?.activeVoiceId ??
    state.config?.defaultVoiceId ??
    installedVoices.find((voice) => voice.isDefault)?.voiceId ??
    installedVoices[0]?.voiceId ??
    "";

  const effectiveVoiceId = selectedVoiceId || defaultVoiceId;

  /**
   * Starts a whole-house paging test after optional confirmation.
   */
  const handleTestPage = (): void => {
    const trimmed = message.trim();
    if (!trimmed) {
      setActionError("Enter a message to page.");
      return;
    }
    if (!pagingEnabled) {
      setActionError("Enable paging before running a whole-house test.");
      return;
    }
    if (!effectiveVoiceId) {
      setActionError("Install at least one voice before testing paging.");
      return;
    }

    if (!awaitingConfirm) {
      setActionError(null);
      setAwaitingConfirm(true);
      return;
    }

    void (async () => {
      setActionError(null);
      try {
        await testPage(trimmed, {
          voiceId: effectiveVoiceId,
          includeChime,
        });
        setAwaitingConfirm(false);
      } catch (error) {
        setActionError(error instanceof Error ? error.message : "Paging test failed");
        setAwaitingConfirm(false);
      }
    })();
  };

  return (
    <div className="rounded-lg border border-border bg-card">
      <div className="border-b border-border px-6 py-4">
        <h2 className="font-medium text-card-foreground">Test Paging</h2>
        <p className="mt-0.5 text-sm text-muted-foreground">
          Type a message and run a whole-house page through HiFi and the paging DAC.
        </p>
      </div>

      <div className="grid gap-4 p-6">
        {actionError ? (
          <div
            className="flex items-start gap-2 rounded-md border border-destructive/40 bg-destructive/10 px-3 py-2 text-sm text-destructive"
            role="alert"
          >
            <CircleAlert className="mt-0.5 size-4 shrink-0" aria-hidden />
            <p>{actionError}</p>
          </div>
        ) : null}

        {!pagingEnabled ? (
          <p className="text-sm text-amber-700 dark:text-amber-300">
            Paging is disabled. Turn it on in the configuration card above before testing.
          </p>
        ) : null}

        <label className="grid gap-2">
          <span className="text-sm font-medium text-foreground">Page message</span>
          <textarea
            className="min-h-28 w-full rounded-md border border-border bg-background px-3 py-2 text-sm text-foreground"
            value={message}
            maxLength={maxTextLength}
            onChange={(event) => setMessage(event.target.value)}
            placeholder="Front door. Visitor at the east entrance."
          />
          <span className="text-xs text-muted-foreground">
            {message.length}/{maxTextLength} characters
          </span>
        </label>

        <label className="grid gap-2">
          <span className="text-sm font-medium text-foreground">Voice</span>
          <select
            className="h-10 w-full rounded-md border border-border bg-background px-3 text-sm text-foreground"
            value={effectiveVoiceId}
            disabled={installedVoices.length === 0}
            onChange={(event) => setSelectedVoiceId(event.target.value)}
          >
            {installedVoices.length === 0 ? (
              <option value="">No installed voices</option>
            ) : (
              installedVoices.map((voice) => (
                <option key={voice.voiceId} value={voice.voiceId}>
                  {voice.displayName} ({voice.accent ?? voice.languageCode}, {voice.quality})
                  {voice.isDefault ? " — default" : ""}
                </option>
              ))
            )}
          </select>
        </label>

        <div className="flex items-center justify-between rounded-md border border-border/60 px-3 py-2">
          <div className="grid gap-0.5">
            <p className="text-sm font-medium text-foreground">Include chime</p>
            <p className="text-xs text-muted-foreground">
              Play the active chime before the spoken message.
            </p>
          </div>
          <Switch
            checked={includeChime}
            aria-label="Include chime before page message"
            onCheckedChange={setIncludeChime}
          />
        </div>

        {awaitingConfirm ? (
          <div className="rounded-md border border-amber-500/40 bg-amber-500/10 px-3 py-3 text-sm text-amber-900 dark:text-amber-200">
            <p className="font-medium">Confirm whole-house page</p>
            <p className="mt-1">
              This will turn on paging across the house and speak your message on all zones.
            </p>
            <div className="mt-3 flex flex-wrap gap-2">
              <Button
                type="button"
                size="sm"
                disabled={isBusy}
                onClick={handleTestPage}
              >
                {isBusy ? <Loader2 className="size-4 animate-spin" /> : <Megaphone className="size-4" />}
                Confirm page now
              </Button>
              <Button
                type="button"
                variant="outline"
                size="sm"
                disabled={isBusy}
                onClick={() => setAwaitingConfirm(false)}
              >
                Cancel
              </Button>
            </div>
          </div>
        ) : (
          <div className="flex flex-wrap gap-2">
            <Button
              type="button"
              variant="outline"
              disabled={isBusy || installedVoices.length === 0}
              onClick={() => {
                const trimmed = message.trim();
                if (!trimmed) {
                  setActionError("Enter a message to test.");
                  return;
                }
                void (async () => {
                  setActionError(null);
                  try {
                    await testDac(trimmed, effectiveVoiceId || undefined);
                  } catch (error) {
                    setActionError(error instanceof Error ? error.message : "DAC test failed");
                  }
                })();
              }}
            >
              Test on paging DAC only
            </Button>
            <Button
              type="button"
              disabled={isBusy || !pagingEnabled || installedVoices.length === 0}
              onClick={handleTestPage}
            >
              {isBusy ? <Loader2 className="size-4 animate-spin" /> : <Megaphone className="size-4" />}
              Test whole-house page
            </Button>
          </div>
        )}
      </div>
    </div>
  );
}
