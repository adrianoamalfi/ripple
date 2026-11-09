import type { DeviceState } from "@/lib/types"

const PHASES = ["uncalibrated", "calibrating", "running"]

function num(v: unknown): v is number {
  return typeof v === "number" && Number.isFinite(v)
}

/** Parse one message from the device. Returns null for anything malformed. */
export function parseState(text: string): DeviceState | null {
  let raw: unknown
  try {
    raw = JSON.parse(text)
  } catch {
    return null
  }
  if (typeof raw !== "object" || raw === null) return null
  const s = raw as Record<string, unknown>
  const wiz = s.wiz as Record<string, unknown> | undefined

  if (
    !num(s.ms) ||
    !num(s.rssi) ||
    !num(s.var) ||
    !num(s.peak) ||
    !num(s.thr) ||
    typeof s.phase !== "string" ||
    !PHASES.includes(s.phase) ||
    !wiz ||
    typeof wiz.state !== "string"
  ) {
    return null
  }
  return raw as DeviceState
}
