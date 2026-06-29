import {
  Airplay,
  Cable,
  DoorOpen,
  Globe,
  HeartPulse,
  Home,
  Megaphone,
  Radio,
  Server,
  ShieldAlert,
  Speaker,
  Usb,
  type LucideIcon,
} from "lucide-react";

/** Lucide icons keyed by module capability id. */
const CAPABILITY_ICONS: Record<string, LucideIcon> = {
  "zone-control": Speaker,
  airplay: Airplay,
  "pcm-routing": Cable,
  paging: Megaphone,
  "contact-detection": DoorOpen,
  "tamper-fault": ShieldAlert,
  "homekit-bridge": Home,
};

/** Lucide icons keyed by platform service name. */
const PLATFORM_SERVICE_ICONS: Record<string, LucideIcon> = {
  "homepi-backend": Globe,
  "homepi-health": HeartPulse,
  "homepi-broker": Radio,
  "homepi-usb-devices": Usb,
};

/**
 * Resolves the Lucide icon that best represents a capability or platform service.
 * @param serviceId - Capability id or platform service name.
 * @returns Matching Lucide icon component.
 */
export function resolveServiceStatusIcon(serviceId: string): LucideIcon {
  return CAPABILITY_ICONS[serviceId] ?? PLATFORM_SERVICE_ICONS[serviceId] ?? Server;
}
