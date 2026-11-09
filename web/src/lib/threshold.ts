import type { DeviceState } from "@/lib/types"

export interface ThresholdRange {
  min: number
  max: number
  step: number
}

/** Slider range for the motion threshold, derived from what the device learned.
 * The floor sits just above the empty-room mean: below that the noise itself
 * would trigger. The ceiling leaves plenty of room to desensitise. */
export function thresholdRange(s: Pick<DeviceState, "mean" | "max" | "thr">): ThresholdRange {
  const min = Math.max(0.5, Math.round((s.mean + 0.5) * 10) / 10)
  const top = Math.max(s.max, s.thr, min) * 3
  const max = Math.max(8, Math.ceil(top))
  return { min, max, step: 0.1 }
}

export function clampThreshold(value: number, range: ThresholdRange): number {
  const v = Math.min(range.max, Math.max(range.min, value))
  return Math.round(v * 10) / 10
}

export type Sensitivity = "very high" | "high" | "balanced" | "low"

/** Describe a threshold relative to the empty-room noise ceiling. */
export function describeSensitivity(
  threshold: number,
  noiseMax: number
): Sensitivity {
  if (noiseMax <= 0) return "balanced"
  const ratio = threshold / noiseMax
  if (ratio < 1) return "very high"
  if (ratio < 1.3) return "high"
  if (ratio < 2) return "balanced"
  return "low"
}
