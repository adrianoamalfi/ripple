import type { MqttUpdate } from "@/lib/types"

// The same rules as the firmware (src/mqtt.cpp), so mistakes show up instantly.
// The device stays the authority: it validates again.

export function validateMqtt(
  update: MqttUpdate,
  current: { enabled: boolean; host: string }
): string | null {
  const host = update.host ?? current.host
  const enabled = update.enabled ?? current.enabled

  if (update.host !== undefined && update.host !== "" && !/^[A-Za-z0-9._-]{1,63}$/.test(update.host)) {
    return "Host may only contain letters, digits, dots, dashes and underscores"
  }
  if (update.port !== undefined) {
    if (!Number.isInteger(update.port) || update.port < 1 || update.port > 65535) {
      return "Port must be between 1 and 65535"
    }
  }
  if (update.user !== undefined) {
    const u = update.user
    if (!/^[\x20-\x7E]{0,31}$/.test(u) || /["\\]/.test(u)) {
      return "Invalid user name (max 31 plain characters)"
    }
  }
  if (update.pass !== undefined) {
    if (update.pass.length > 63) return "Password too long (max 63)"
    if ([...update.pass].some((c) => c.charCodeAt(0) < 0x20)) return "Invalid password"
  }
  if (update.prefix !== undefined) {
    const p = update.prefix
    if (!/^[A-Za-z0-9_/-]{1,31}$/.test(p) || p.startsWith("/") || p.endsWith("/")) {
      return "Discovery prefix must be letters, digits, _ - or / (max 31)"
    }
  }
  if (enabled && !host) return "Host is required"
  return null
}
