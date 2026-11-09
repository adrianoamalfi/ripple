import { describe, expect, it } from "vitest"

import {
  MAX_BUCKETS,
  motionRanges,
  pushSample,
  secondsSinceMotion,
  toChartPoints,
  type Bucket,
} from "@/lib/history"
import type { DeviceState } from "@/lib/types"

function state(over: Partial<DeviceState> = {}): DeviceState {
  return {
    v: 1, fw: "t", ms: 0, rssi: -50, ok: 1, var: 1, peak: 1, thr: 3, mean: 1,
    sigma: 0.3, max: 2, score: 0, phase: "running", cal: 1, motion: 0,
    presence: 0, source: "wizard", saved: 1, heap: 30000,
    wiz: { state: "idle", stage: 1, name: "", left: 0 }, ...over,
  }
}

describe("pushSample", () => {
  it("keeps the peak of messages inside the same second", () => {
    let h: Bucket[] = []
    h = pushSample(h, state({ peak: 1.2 }), 10_000)
    h = pushSample(h, state({ peak: 4.5 }), 10_300)
    h = pushSample(h, state({ peak: 2.0 }), 10_700)
    expect(h).toHaveLength(1)
    expect(h[0].peak).toBe(4.5)
  })

  it("remembers motion seen anywhere in the second", () => {
    let h: Bucket[] = []
    h = pushSample(h, state({ motion: 1 }), 10_000)
    h = pushSample(h, state({ motion: 0 }), 10_400)
    expect(h[0].motion).toBe(true)
  })

  it("starts a new bucket each second", () => {
    let h: Bucket[] = []
    h = pushSample(h, state(), 10_000)
    h = pushSample(h, state(), 11_000)
    expect(h.map((b) => b.t)).toEqual([10, 11])
  })

  it("has no threshold or motion while not running", () => {
    const h = pushSample([], state({ phase: "calibrating", thr: 0, motion: 1 }), 5_000)
    expect(h[0].thr).toBeNull()
    expect(h[0].motion).toBe(false)
  })

  it("has no rssi when the link is down", () => {
    const h = pushSample([], state({ ok: 0 }), 5_000)
    expect(h[0].rssi).toBeNull()
  })

  it("is bounded", () => {
    let h: Bucket[] = []
    for (let i = 0; i < MAX_BUCKETS + 50; i++) h = pushSample(h, state(), i * 1000)
    expect(h).toHaveLength(MAX_BUCKETS)
    expect(h[h.length - 1].t).toBe(MAX_BUCKETS + 49)
  })
})

describe("toChartPoints", () => {
  it("covers the window with the newest point at x = 0 and leaves gaps empty", () => {
    let h: Bucket[] = []
    h = pushSample(h, state({ peak: 2 }), 100_000)
    h = pushSample(h, state({ peak: 3 }), 103_000)
    const pts = toChartPoints(h, 5, 103_500)
    expect(pts).toHaveLength(6)
    expect(pts[0].x).toBe(-5)
    expect(pts[5]).toMatchObject({ x: 0, peak: 3 })
    expect(pts[2]).toMatchObject({ x: -3, peak: 2 })
    expect(pts[3].peak).toBeNull() // 101 s: no data
  })
})

describe("secondsSinceMotion", () => {
  it("is null when there was never motion", () => {
    const h = pushSample([], state(), 10_000)
    expect(secondsSinceMotion(h, 20_000)).toBeNull()
  })
  it("counts from the latest motion", () => {
    let h: Bucket[] = []
    h = pushSample(h, state({ motion: 1 }), 10_000)
    h = pushSample(h, state({ motion: 0 }), 15_000)
    expect(secondsSinceMotion(h, 18_500)).toBe(8)
  })
})

describe("motionRanges", () => {
  it("groups consecutive motion seconds, including one still open at the end", () => {
    const mk = (motion: boolean[]) =>
      motion.map((m, i) => ({ x: i - motion.length + 1, peak: 1, thr: 2, motion: m }))
    expect(motionRanges(mk([false, true, true, false, true]))).toEqual([[-3, -1], [0, 0]])
    expect(motionRanges(mk([false, false]))).toEqual([])
  })
})
