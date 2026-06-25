/**
 * Returns true when a value looks like a Shairport persistent track id (0x…).
 * @param value - Candidate metadata value.
 * @returns Whether the value is a persistent id.
 */
export function isPersistentIdLike(value?: string): boolean {
  const trimmed = value?.trim();
  if (!trimmed || trimmed.length < 4 || !trimmed.startsWith("0x")) {
    return false;
  }
  return /^0x[0-9a-f]+$/i.test(trimmed);
}

/**
 * Maps Apple internal model identifiers to friendly device names.
 * @param model - Raw model string from Shairport `cmod`.
 * @returns Friendly label when known.
 */
function friendlyAppleModel(model?: string): string | undefined {
  const trimmed = model?.trim();
  if (!trimmed) {
    return undefined;
  }
  const known: Record<string, string> = {
    "iPhone15,2": "iPhone 14 Pro",
    "iPhone15,3": "iPhone 14 Pro Max",
    "iPhone14,7": "iPhone 14",
    "iPhone14,8": "iPhone 14 Plus",
    "iPhone16,1": "iPhone 15 Pro",
    "iPhone16,2": "iPhone 15 Pro Max",
  };
  return known[trimmed] ?? trimmed;
}

/**
 * Returns a display-safe client name, filtering persistent ids and track ids.
 * @param clientName - Raw client name from metadata.
 * @param clientModel - Fallback client model.
 * @param trackId - Current track id for cross-checking.
 * @returns Sanitized client label or undefined.
 */
export function sanitizeClientName(
  clientName?: string,
  clientModel?: string,
  trackId?: string
): string | undefined {
  const name = clientName?.trim();
  if (name && !isPersistentIdLike(name) && name !== trackId?.trim()) {
    return name;
  }
  const model = clientModel?.trim();
  if (model && !isPersistentIdLike(model) && model !== trackId?.trim()) {
    return friendlyAppleModel(model);
  }
  return undefined;
}

/**
 * Returns true when metadata payload includes displayable text fields.
 * @param payload - Metadata snapshot or field payload.
 * @returns Whether any known text field is non-empty.
 */
export function hasMetadataText(payload: Record<string, unknown>): boolean {
  return ["title", "artist", "album", "clientName", "client_name"].some((key) => {
    const value = payload[key];
    return typeof value === "string" && value.trim().length > 0;
  });
}

/**
 * Returns true when now-playing UI should remain visible.
 * @param payload - Metadata snapshot payload.
 * @returns Whether the payload carries enough content to show.
 */
export function hasNowPlayingDisplayContent(payload: Record<string, unknown>): boolean {
  if (hasMetadataText(payload)) {
    return true;
  }
  if (payload.hasCoverArt === true) {
    return true;
  }
  if (typeof payload.trackId === "string" && payload.trackId.length > 0) {
    return true;
  }
  if (payload.playing === true && typeof payload.durationMs === "number" && payload.durationMs > 0) {
    return true;
  }
  return false;
}

/**
 * Picks the best display title from metadata fields.
 * @param payload - Metadata fields.
 * @returns Display title string.
 */
export function pickDisplayTitle(payload: {
  title?: string;
  track?: string;
  artist?: string;
  clientName?: string;
  clientModel?: string;
  album?: string;
}): string {
  const trackValue = payload.track?.trim();
  const title =
    payload.title?.trim() ||
    (trackValue && trackValue !== "Now Playing" ? trackValue : undefined);
  if (title) {
    return title;
  }
  const clientName = sanitizeClientName(payload.clientName, payload.clientModel);
  if (clientName) {
    return clientName;
  }
  return "Now Playing";
}
