import { formatServiceHealthDetail, type ServiceHealthDetailInput } from "@/lib/status-display.js";

/**
 * Props for a health detail subtitle under a service name.
 */
export interface ServiceHealthDetailProps extends ServiceHealthDetailInput {}

/**
 * Renders evidence or status detail beneath a service or capability name.
 * @param props - Component props.
 * @returns Detail element or null.
 */
export function ServiceHealthDetail(props: ServiceHealthDetailProps): React.JSX.Element | null {
  const detail = formatServiceHealthDetail(props);
  if (!detail) {
    return null;
  }

  return <p className="mt-1 text-sm text-muted-foreground">{detail}</p>;
}
