import { useState } from "react";
import { CircleAlert, Loader2 } from "lucide-react";

import { Button } from "@/components/ui/button.js";
import type { UsePagingModuleResult } from "@/hooks/use-paging-module.js";

/**
 * Props for paging idle policy selector card.
 */
export interface PagingIdlePolicyCardProps {
  /** Shared paging module hook result. */
  paging: UsePagingModuleResult;
}

/**
 * Idle policy control for warm/cold resource behavior.
 * @param props - Component props.
 * @returns Idle policy card element.
 */
export function PagingIdlePolicyCard({ paging }: PagingIdlePolicyCardProps): React.JSX.Element {
  const { state, setIdlePolicy } = paging;
  const [actionError, setActionError] = useState<string | null>(null);
  const policy = state.config?.idlePolicy ?? "always_warm";
  const isSaving = Boolean(state.pendingActions.updateConfig);

  return (
    <div className="rounded-lg border border-border bg-card">
      <div className="border-b border-border px-6 py-4">
        <h2 className="font-medium text-card-foreground">Idle Policy</h2>
        <p className="mt-0.5 text-sm text-muted-foreground">
          Choose whether paging resources stay warm or unload after inactivity.
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

        <div className="flex flex-wrap gap-2">
          <Button
            type="button"
            variant={policy === "always_warm" ? "default" : "outline"}
            disabled={isSaving}
            onClick={() => {
              void (async () => {
                setActionError(null);
                try {
                  await setIdlePolicy("always_warm");
                } catch (error) {
                  setActionError(error instanceof Error ? error.message : "Failed to update policy");
                }
              })();
            }}
          >
            {isSaving && policy === "always_warm" ? <Loader2 className="size-4 animate-spin" /> : null}
            Always warm
          </Button>
          <Button
            type="button"
            variant={policy === "warm_with_timeout" ? "default" : "outline"}
            disabled={isSaving}
            onClick={() => {
              void (async () => {
                setActionError(null);
                try {
                  await setIdlePolicy("warm_with_timeout");
                } catch (error) {
                  setActionError(error instanceof Error ? error.message : "Failed to update policy");
                }
              })();
            }}
          >
            {isSaving && policy === "warm_with_timeout" ? (
              <Loader2 className="size-4 animate-spin" />
            ) : null}
            Warm with timeout
          </Button>
        </div>

        <p className="text-xs text-muted-foreground">
          Current idle timeout: {(state.config?.idleWarmTimeoutMs ?? 0) / 1000}s.
        </p>
      </div>
    </div>
  );
}
