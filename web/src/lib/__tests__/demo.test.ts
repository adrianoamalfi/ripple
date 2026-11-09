import { describe, expect, it } from "vitest"

import { DemoDevice } from "@/lib/demo"

function run(d: DemoDevice, seconds: number) {
  let s = d.tick(0.2)
  for (let t = 0.2; t < seconds; t += 0.2) s = d.tick(0.2)
  return s
}

describe("DemoDevice", () => {
  it("calibrates by itself at start, without saving", () => {
    const d = new DemoDevice()
    expect(d.tick(0.2).phase).toBe("calibrating")
    const s = run(d, 8)
    expect(s.phase).toBe("running")
    expect(s.source).toBe("quick")
    expect(s.saved).toBe(0)
  })

  it("walks through the four wizard steps and saves a result", () => {
    const d = new DemoDevice({ countdownS: 2, recordS: 3 })
    run(d, 8)
    d.handle({ cmd: "wizard_start" })
    for (let stage = 1; stage <= 4; stage++) {
      let s = d.tick(0.2)
      expect(s.wiz).toMatchObject({ state: "waiting", stage })
      d.handle({ cmd: "wizard_ready" })
      s = d.tick(0.2)
      expect(s.wiz.state).toBe("countdown")
      s = run(d, 2.4)
      expect(s.wiz.state).toBe("recording")
      run(d, 3.4)
    }
    const s = d.tick(0.2)
    expect(s.wiz.state).toBe("done")
    expect(s.source).toBe("wizard")
    expect(s.saved).toBe(1)
    expect(s.res?.quality).toBe("good")
  })

  it("reports a poor result when walking is invisible", () => {
    const d = new DemoDevice({ quality: "poor", countdownS: 1, recordS: 2 })
    run(d, 8)
    d.handle({ cmd: "wizard_start" })
    for (let i = 0; i < 4; i++) {
      d.tick(0.2)
      d.handle({ cmd: "wizard_ready" })
      run(d, 4)
    }
    expect(d.tick(0.2).res?.quality).toBe("poor")
  })

  it("applies a manual threshold only while running, and saves it", () => {
    const d = new DemoDevice()
    d.handle({ cmd: "set_threshold", value: 9 })
    expect(d.tick(0.2).thr).toBe(0) // still calibrating: ignored
    run(d, 8)
    d.handle({ cmd: "set_threshold", value: 5.5 })
    const s = d.tick(0.2)
    expect(s.thr).toBe(5.5)
    expect(s.source).toBe("manual")
    expect(s.saved).toBe(1)
  })

  it("resets the calibration", () => {
    const d = new DemoDevice()
    run(d, 8)
    d.handle({ cmd: "reset_calibration" })
    const s = d.tick(0.2)
    expect(s.phase).toBe("calibrating")
    expect(s.saved).toBe(0)
  })
})
