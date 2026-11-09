import { describe, expect, it } from "vitest"

import { formatAgo, formatDuration, signalQuality } from "@/lib/format"
import { parseState } from "@/lib/protocol"
import { describeStatus } from "@/lib/status"
import { clampThreshold, describeSensitivity, thresholdRange } from "@/lib/threshold"
import type { DeviceState } from "@/lib/types"

const base: DeviceState = {
  v: 1, fw: "t", ms: 0, rssi: -50, ok: 1, var: 1, peak: 1, thr: 3, mean: 1,
  sigma: 0.3, max: 2, score: 0, phase: "running", cal: 1, motion: 0,
  presence: 0, source: "wizard", saved: 1, heap: 30000,
  wiz: { state: "idle", stage: 1, name: "", left: 0 },
}

describe("format", () => {
  it("formats durations", () => {
    expect(formatDuration(5)).toBe("5s")
    expect(formatDuration(65)).toBe("1m 05s")
    expect(formatDuration(3725)).toBe("1h 02m")
    expect(formatDuration(3 * 86400 + 5 * 3600)).toBe("3d 5h")
  })
  it("formats relative times", () => {
    expect(formatAgo(null)).toBe("not seen yet")
    expect(formatAgo(1)).toBe("just now")
    expect(formatAgo(42)).toBe("42s ago")
  })
  it("rates the signal", () => {
    expect(signalQuality(-45).bars).toBe(4)
    expect(signalQuality(-70).bars).toBe(2)
    expect(signalQuality(-88).bars).toBe(1)
    expect(signalQuality(null).bars).toBe(0)
  })
})

describe("parseState", () => {
  const good = JSON.stringify(base)
  it("accepts a device message", () => {
    expect(parseState(good)?.rssi).toBe(-50)
  })
  it("rejects garbage and malformed messages", () => {
    expect(parseState("not json")).toBeNull()
    expect(parseState("null")).toBeNull()
    expect(parseState(JSON.stringify({ ...base, phase: "bogus" }))).toBeNull()
    expect(parseState(JSON.stringify({ ...base, wiz: undefined }))).toBeNull()
    expect(parseState(JSON.stringify({ ...base, thr: "3" }))).toBeNull()
  })
})

describe("describeStatus", () => {
  it("covers every state", () => {
    expect(describeStatus(null, "connecting", null).kind).toBe("offline")
    expect(describeStatus(base, "stalled", null).kind).toBe("offline")
    expect(describeStatus({ ...base, phase: "calibrating" }, "open", null).kind).toBe("calibrating")
    expect(describeStatus({ ...base, phase: "uncalibrated" }, "open", null).kind).toBe("uncalibrated")
    expect(describeStatus({ ...base, motion: 1, presence: 1 }, "open", 0).kind).toBe("motion")
    expect(describeStatus({ ...base, presence: 1 }, "open", 12)).toMatchObject({
      kind: "presence",
      subtitle: "Last motion 12s ago",
    })
    expect(describeStatus(base, "open", null)).toMatchObject({
      kind: "empty",
      subtitle: "No motion seen yet",
    })
  })
})

describe("threshold", () => {
  it("keeps the slider floor above the empty-room mean", () => {
    const r = thresholdRange({ mean: 1.1, max: 2.4, thr: 2.8 })
    expect(r.min).toBeGreaterThanOrEqual(1.6)
    expect(r.max).toBeGreaterThan(r.min)
  })
  it("clamps and rounds", () => {
    const r = { min: 1, max: 8, step: 0.1 }
    expect(clampThreshold(0.2, r)).toBe(1)
    expect(clampThreshold(99, r)).toBe(8)
    expect(clampThreshold(3.14159, r)).toBe(3.1)
  })
  it("describes sensitivity relative to the noise", () => {
    expect(describeSensitivity(2, 2.4)).toBe("very high")
    expect(describeSensitivity(2.76, 2.4)).toBe("high")
    expect(describeSensitivity(3.6, 2.4)).toBe("balanced")
    expect(describeSensitivity(6, 2.4)).toBe("low")
  })
})
