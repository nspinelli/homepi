import { Check, CircleAlert, LogOut, Speaker } from "lucide-react";
import { useSyncExternalStore } from "react";
import { createPortal } from "react-dom";

import {
  dismissToast,
  getToasts,
  parseToastDisplayContent,
  subscribeToasts,
  type ToastRecord,
} from "@/lib/toast.js";
import { cn } from "@/lib/utils.js";

/**
 * iOS-style pill toast with avatar icon and status badge.
 */
function ToastPill({ toast }: { toast: ToastRecord }): React.JSX.Element {
  const { title, subtitle } = parseToastDisplayContent(toast);
  const isError = toast.variant === "error";

  return (
    <button
      type="button"
      role="status"
      className={cn(
        "pointer-events-auto flex max-w-[min(100vw-2rem,22rem)] items-center gap-3 rounded-full border px-3 py-2.5 text-left",
        "text-foreground opacity-100",
        "shadow-[var(--toast-shadow)] backdrop-blur-xl backdrop-saturate-150",
        "transition-transform active:scale-[0.98]"
      )}
      style={{
        backgroundColor: "var(--toast-bg)",
        borderColor: "var(--toast-border)",
        boxShadow: "var(--toast-shadow)",
      }}
      onClick={() => dismissToast(toast.id)}
    >
      <div className="relative h-10 w-10 shrink-0">
        <div
          className={cn(
            "flex h-10 w-10 items-center justify-center rounded-full",
            isError ? "bg-destructive/15" : "bg-zone-accent/15"
          )}
        >
          <Speaker
            className={cn("h-5 w-5", isError ? "text-destructive" : "text-zone-accent")}
            aria-hidden
          />
        </div>
        <div
          className={cn(
            "absolute -right-0.5 -bottom-0.5 flex h-[1.125rem] w-[1.125rem] items-center justify-center rounded-[0.3rem]",
            isError ? "bg-destructive" : "bg-success"
          )}
          style={{ boxShadow: "0 0 0 2px var(--toast-bg)" }}
          aria-hidden
        >
          {isError ? (
            <CircleAlert className="h-2.5 w-2.5 text-white" strokeWidth={2.5} />
          ) : (
            <Check className="h-2.5 w-2.5 text-white" strokeWidth={3} />
          )}
        </div>
      </div>

      <div className="min-w-0 flex-1 pr-1">
        <p
          className="truncate text-[0.9375rem] leading-tight font-semibold"
          style={{ color: "var(--toast-title)" }}
        >
          {title}
        </p>
        <p
          className="mt-0.5 flex items-center gap-1 text-xs leading-tight"
          style={{ color: "var(--toast-subtitle)" }}
        >
          {isError ? <LogOut className="h-3 w-3 shrink-0 opacity-80" aria-hidden /> : null}
          <span className="truncate">{subtitle}</span>
        </p>
      </div>
    </button>
  );
}

/**
 * Renders transient iOS-style toast notifications (auto-dismiss handled in {@link showToast}).
 */
export function Toaster(): React.JSX.Element | null {
  const toasts = useSyncExternalStore(subscribeToasts, getToasts, getToasts);

  if (typeof document === "undefined" || toasts.length === 0) {
    return null;
  }

  return createPortal(
    <div
      className="pointer-events-none fixed inset-x-0 top-4 z-[9999] flex flex-col items-center gap-2 px-4 sm:top-5"
      aria-live="polite"
      aria-relevant="additions removals"
    >
      {toasts.map((toast) => (
        <ToastPill key={toast.id} toast={toast} />
      ))}
    </div>,
    document.body
  );
}
