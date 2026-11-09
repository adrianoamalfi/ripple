import type { DeviceState } from "@/lib/types"

/** One second of signal. The device sends ~5 messages a second; we keep the
 * peak per second so short bursts never disappear from the chart. */
export interface Bucket {
  t: number // unix seconds
  peak: number
  thr: number | null // null while there is no threshold (not calibrated)
  rssi: number | null
  motion: boolean
  presence: boolean
}

export const MAX_BUCKETS = 15 * 60

export function pushSample(
  history: Bucket[],
  s: DeviceState,
  nowMs: number
): Bucket[] {
  const t = Math.floor(nowMs / 1000)
  const running = s.phase === "running"
  const sample = {
    peak: s.peak,
    thr: running && s.thr > 0 ? s.thr : null,
    rssi: s.ok ? s.rssi : null,
    motion: running && s.motion === 1,
    presence: running && s.presence === 1,
  }

  const last = history[history.length - 1]
  if (last && last.t === t) {
    const merged: Bucket = {
      t,
      peak: Math.max(last.peak, sample.peak),
      thr: sample.thr,
      rssi: sample.rssi ?? last.rssi,
      motion: last.motion || sample.motion,
      presence: sample.presence,
    }
    return [...history.slice(0, -1), merged]
  }

  const next = [...history, { t, ...sample }]
  return next.length > MAX_BUCKETS ? next.slice(next.length - MAX_BUCKETS) : next
}

/** Buckets of the last `seconds` seconds, as chart points where x is the age
 * in seconds (0 = now, negative = past). Gaps stay gaps. */
export interface ChartPoint {
  x: number
  peak: number | null
  thr: number | null
  motion: boolean
}

export function toChartPoints(
  history: Bucket[],
  seconds: number,
  nowMs: number
): ChartPoint[] {
  const now = Math.floor(nowMs / 1000)
  const byT = new Map(history.map((b) => [b.t, b]))
  const points: ChartPoint[] = []
  for (let age = seconds; age >= 0; age--) {
    const b = byT.get(now - age)
    points.push({
      x: age === 0 ? 0 : -age, // avoid -0
      peak: b ? b.peak : null,
      thr: b ? b.thr : null,
      motion: b ? b.motion : false,
    })
  }
  return points
}

/** Seconds since motion was last seen, or null if not in the history. */
export function secondsSinceMotion(
  history: Bucket[],
  nowMs: number
): number | null {
  const now = Math.floor(nowMs / 1000)
  for (let i = history.length - 1; i >= 0; i--) {
    if (history[i].motion) return Math.max(0, now - history[i].t)
  }
  return null
}

/** Contiguous runs of motion as [startX, endX] pairs, for chart shading. */
export function motionRanges(points: ChartPoint[]): Array<[number, number]> {
  const ranges: Array<[number, number]> = []
  let start: number | null = null
  for (const p of points) {
    if (p.motion && start === null) start = p.x
    if (!p.motion && start !== null) {
      ranges.push([start, p.x])
      start = null
    }
  }
  if (start !== null) ranges.push([start, points[points.length - 1].x])
  return ranges
}
