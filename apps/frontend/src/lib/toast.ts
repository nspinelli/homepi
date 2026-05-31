/**
 * Toast notification variant.
 */
export type ToastVariant = "success" | "error";

/**
 * A single toast notification record.
 */
export interface ToastRecord {
  /** Unique toast id. */
  id: string;
  /** Message shown to the user. */
  message: string;
  /** Visual style. */
  variant: ToastVariant;
}

/**
 * Parsed title and subtitle for pill-style toast layout.
 */
export interface ToastDisplayContent {
  /** Primary line (bold). */
  title: string;
  /** Secondary line below the title. */
  subtitle: string;
}

type ToastListener = () => void;

const TOAST_DURATION_MS = 5000;

let toasts: ToastRecord[] = [];
const listeners = new Set<ToastListener>();

/**
 * Creates a unique toast id (works on HTTP without crypto.randomUUID).
 * @returns Unique id string.
 */
function createToastId(): string {
  if (typeof crypto !== "undefined" && typeof crypto.randomUUID === "function") {
    return crypto.randomUUID();
  }
  return `toast-${Date.now()}-${Math.random().toString(36).slice(2, 9)}`;
}

/**
 * Notifies all subscribers that the toast list changed.
 */
function notifyListeners(): void {
  for (const listener of listeners) {
    listener();
  }
}

/**
 * Returns the current toast list snapshot.
 * @returns Active toasts.
 */
export function getToasts(): ToastRecord[] {
  return toasts;
}

/**
 * Subscribes to toast list changes (for useSyncExternalStore).
 * @param onStoreChange - Called when toasts are added or removed.
 * @returns Unsubscribe function.
 */
export function subscribeToasts(onStoreChange: () => void): () => void {
  listeners.add(onStoreChange);
  return () => {
    listeners.delete(onStoreChange);
  };
}

/**
 * Shows a toast that auto-dismisses after five seconds.
 * @param message - User-visible message.
 * @param variant - Success or error styling.
 * @returns Toast id.
 */
export function showToast(message: string, variant: ToastVariant = "success"): string {
  const id = createToastId();
  toasts = [...toasts, { id, message, variant }];
  notifyListeners();

  window.setTimeout(() => {
    dismissToast(id);
  }, TOAST_DURATION_MS);

  return id;
}

/**
 * Removes a toast immediately.
 * @param id - Toast id to dismiss.
 */
export function dismissToast(id: string): void {
  const next = toasts.filter((toast) => toast.id !== id);
  if (next.length === toasts.length) {
    return;
  }
  toasts = next;
  notifyListeners();
}

/**
 * Splits a toast message into a title and subtitle for display.
 * @param toast - Toast record.
 * @returns Title and subtitle strings.
 */
export function parseToastDisplayContent(toast: ToastRecord): ToastDisplayContent {
  if (toast.variant === "error") {
    return {
      title: "Action failed",
      subtitle: toast.message,
    };
  }

  const zoneMatch = toast.message.match(/^Zone (\d+)\s+(.+)$/i);
  if (zoneMatch) {
    return {
      title: `Zone ${zoneMatch[1]}`,
      subtitle: zoneMatch[2],
    };
  }

  return {
    title: "Home Audio",
    subtitle: toast.message,
  };
}
