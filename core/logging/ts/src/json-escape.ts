/**
 * Escapes a string for safe inclusion in JSON string values.
 * Handles quotes, backslashes, control characters, and newlines.
 * @param value - Raw string value.
 * @returns Escaped string without surrounding quotes.
 */
export function escapeJsonString(value: string): string {
  let result = "";
  for (let i = 0; i < value.length; i++) {
    const ch = value[i];
    const code = ch.charCodeAt(0);
    switch (ch) {
      case '"':
        result += '\\"';
        break;
      case "\\":
        result += "\\\\";
        break;
      case "\b":
        result += "\\b";
        break;
      case "\f":
        result += "\\f";
        break;
      case "\n":
        result += "\\n";
        break;
      case "\r":
        result += "\\r";
        break;
      case "\t":
        result += "\\t";
        break;
      default:
        if (code < 0x20) {
          result += `\\u${code.toString(16).padStart(4, "0")}`;
        } else {
          result += ch;
        }
    }
  }
  return result;
}

/**
 * Serializes a log message to single-line JSON with safe escaping.
 * @param message - Structured log message.
 * @returns Single-line JSON string.
 */
export function serializeLogMessage(message: Record<string, unknown>): string {
  return JSON.stringify(message, (_key, value) => {
    if (typeof value === "string") {
      return value;
    }
    return value;
  });
}
