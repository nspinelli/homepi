import { useState } from "react";
import { CircleAlert, KeyRound, Loader2, Trash2 } from "lucide-react";

import { Button } from "@/components/ui/button.js";
import { Input } from "@/components/ui/input.js";
import { Label } from "@/components/ui/label.js";
import type { UsePagingModuleResult } from "@/hooks/use-paging-module.js";

/**
 * Props for paging API key card.
 */
export interface PagingApiKeyCardProps {
  /** Shared paging module hook result. */
  paging: UsePagingModuleResult;
}

/**
 * Paging API key management card for Audio settings tab.
 * @param props - Component props.
 * @returns API key card element.
 */
export function PagingApiKeyCard({ paging }: PagingApiKeyCardProps): React.JSX.Element {
  const { state, setApiKey, clearApiKey } = paging;
  const [apiKey, setApiKeyValue] = useState("");
  const [confirmApiKey, setConfirmApiKey] = useState("");
  const [actionError, setActionError] = useState<string | null>(null);
  const isConfigured = state.apiKeyInfo?.pagingApiKeyConfigured ?? false;
  const isSaving = Boolean(state.pendingActions.setApiKey);
  const isClearing = Boolean(state.pendingActions.clearApiKey);

  return (
    <div className="rounded-lg border border-border bg-card">
      <div className="border-b border-border px-6 py-4">
        <h2 className="font-medium text-card-foreground">Paging API Key</h2>
        <p className="mt-0.5 text-sm text-muted-foreground">
          Choose a password for external paging automation endpoints such as Shortcuts.
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

        <div className="rounded-md border border-border/60 px-3 py-3">
          <p className="text-sm font-medium text-foreground">
            Status: {isConfigured ? "Configured" : "Not configured"}
          </p>
          {isConfigured ? (
            <p className="mt-1 text-xs text-muted-foreground">
              Hint: {state.apiKeyInfo?.pagingApiKeyPrefix ?? "—"}
            </p>
          ) : null}
        </div>

        <div className="grid gap-3">
          <div className="grid gap-2">
            <Label htmlFor="paging-api-key">API Key</Label>
            <Input
              id="paging-api-key"
              type="password"
              autoComplete="new-password"
              value={apiKey}
              disabled={state.loadingApiKeyInfo || isSaving || isClearing}
              onChange={(event) => setApiKeyValue(event.target.value)}
              placeholder="Enter at least 8 characters"
            />
          </div>
          <div className="grid gap-2">
            <Label htmlFor="paging-api-key-confirm">Confirm API Key</Label>
            <Input
              id="paging-api-key-confirm"
              type="password"
              autoComplete="new-password"
              value={confirmApiKey}
              disabled={state.loadingApiKeyInfo || isSaving || isClearing}
              onChange={(event) => setConfirmApiKey(event.target.value)}
              placeholder="Re-enter the same key"
            />
          </div>
        </div>

        <div className="flex flex-wrap justify-end gap-2">
          {isConfigured ? (
            <Button
              type="button"
              variant="outline"
              disabled={state.loadingApiKeyInfo || isSaving || isClearing}
              onClick={() => {
                void (async () => {
                  setActionError(null);
                  try {
                    await clearApiKey();
                    setApiKeyValue("");
                    setConfirmApiKey("");
                  } catch (error) {
                    setActionError(
                      error instanceof Error ? error.message : "Failed to clear paging API key"
                    );
                  }
                })();
              }}
            >
              {isClearing ? (
                <>
                  <Loader2 className="mr-2 size-4 animate-spin" />
                  Clearing…
                </>
              ) : (
                <>
                  <Trash2 className="mr-2 size-4" />
                  Clear Key
                </>
              )}
            </Button>
          ) : null}
          <Button
            type="button"
            disabled={state.loadingApiKeyInfo || isSaving || isClearing}
            onClick={() => {
              void (async () => {
                setActionError(null);
                if (apiKey !== confirmApiKey) {
                  setActionError("API keys do not match");
                  return;
                }
                try {
                  await setApiKey(apiKey);
                  setApiKeyValue("");
                  setConfirmApiKey("");
                } catch (error) {
                  setActionError(
                    error instanceof Error ? error.message : "Failed to save paging API key"
                  );
                }
              })();
            }}
          >
            {isSaving ? (
              <>
                <Loader2 className="mr-2 size-4 animate-spin" />
                Saving…
              </>
            ) : (
              <>
                <KeyRound className="mr-2 size-4" />
                {isConfigured ? "Update Key" : "Save Key"}
              </>
            )}
          </Button>
        </div>
      </div>
    </div>
  );
}
