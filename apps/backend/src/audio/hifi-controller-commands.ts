/**
 * Escapes a string for Hi-Fi2 quoted protocol values.
 * @param value - Raw string.
 * @returns Escaped value without surrounding quotes.
 */
function escapeProtocolString(value: string): string {
  return value.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\*/g, "\\*");
}

/**
 * Builds a Hi-Fi2 controller network name SET command.
 * @param name - Controller display name.
 * @returns Command string including leading *.
 */
export function buildNetNameCommand(name: string): string {
  return `*NETNAME"${escapeProtocolString(name)}"`;
}
