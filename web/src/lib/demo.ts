// A simulated device, so the dashboard can be developed and demoed without
// hardware: `npm run dev` (no VITE_DEVICE) or `?demo` in the URL. It mirrors
// the firmware's behaviour: quick calibration, the four-step wizard, saved
// thresholds, motion with hysteresis, presence timeout.

import type {
  CalibrationResult,
  Command,
  ConnectionStatus,
  DeviceState,
  Quality,
  Transport,
  WizardState,
} from "@/lib/types"

export interface DemoOptions {
  /** Outcome of the guided calibration. */
  quality?: Quality
  /** Wizard timings in seconds (the firmware uses 10 / 30). */
  countdownS?: number
  recordS?: number
  /** Seconds of simulated motion every `motionEveryS` seconds. */
  motionEveryS?: number
  seed?: number
}

const STAGE_NAMES = [
  "Leave the room (empty)",
  "Walk around the room",
  "Sit still in the room",
  "Leave the room again (empty)",
]

export class DemoDevice {
  private o: Required<DemoOptions>
  private t = 0 // simulated seconds since boot
  private rng: () => number

  private phase: DeviceState["phase"] = "calibrating"
  private cal = 0
  private mean = 0.9
  private sigma = 0.35
  private max = 2.4
  private thr = 3.1
  private source = "none"
  private saved = 0
  private res: CalibrationResult | undefined

  private motion = false
  private lastMotionT = -1e9
  private presenceHoldS = 12
  private scriptedUntil = 0

  private wizState: WizardState = "idle"
  private wizStage = 1
  private wizEnd = 0

  private rssi = -61
  private variance = 0.8
  private peak = 0

  constructor(options: DemoOptions = {}) {
    this.o = {
      quality: "good",
      countdownS: 5,
      recordS: 10,
      motionEveryS: 28,
      seed: 7,
      ...options,
    }
    let s = this.o.seed >>> 0
    this.rng = () => {
      s = (Math.imul(s, 1664525) + 1013904223) >>> 0
      return s / 2 ** 32
    }
  }

  /** Apply a command, like the firmware's command queue. */
  handle(c: Command) {
    switch (c.cmd) {
      case "wizard_start":
        this.wizState = "waiting"
        this.wizStage = 1
        break
      case "wizard_ready":
        if (this.wizState === "waiting") {
          this.wizState = "countdown"
          this.wizEnd = this.t + this.o.countdownS
        }
        break
      case "wizard_cancel":
        this.wizState = "idle"
        break
      case "quick_calibrate":
        this.startQuick("quick", 1)
        break
      case "reset_calibration":
        this.startQuick("none", 0)
        break
      case "set_threshold":
        if (this.phase === "running") {
          this.thr = c.value
          this.source = "manual"
          this.saved = 1
        }
        break
    }
  }

  private startQuick(source: string, saved: number) {
    this.phase = "calibrating"
    this.cal = 0
    this.source = source
    this.saved = saved
    this.res = undefined
    this.wizState = "idle"
  }

  private activity(): "empty" | "moving" | "still" {
    if (this.wizState === "countdown") return "moving" // walking into position
    if (this.wizState === "recording") {
      if (this.wizStage === 2) return "moving"
      return "empty" // stage 3 (still) looks like an empty room
    }
    return this.t < this.scriptedUntil ? "moving" : "empty"
  }

  /** Advance the simulation by `dt` seconds and return the new state. */
  tick(dt: number): DeviceState {
    this.t += dt
    this.advanceWizard()

    const act = this.activity()
    const poor = this.o.quality === "poor"
    const base = 0.75 + 0.35 * Math.sin(this.t / 3.1) + (this.rng() - 0.5) * 0.5
    const burst = act === "moving" ? (poor ? 1.4 : 5.5) * (0.4 + this.rng()) : 0
    this.variance = Math.max(0.05, base + burst)
    this.peak = this.variance * (1 + this.rng() * 0.15)
    this.rssi = Math.round(this.rssi + (-61 - this.rssi) * 0.1 + (this.rng() - 0.5) * 2)

    if (this.phase === "calibrating") {
      this.cal = Math.min(1, this.cal + dt / 6)
      if (this.cal >= 1) {
        this.phase = "running"
        if (this.source === "none") this.source = "quick"
      }
    }

    if (this.phase === "running") {
      if (!this.motion && this.variance > this.thr) this.motion = true
      else if (this.motion && this.variance < this.mean + 0.5 * (this.thr - this.mean))
        this.motion = false
      if (this.motion) this.lastMotionT = this.t
      // Scripted episodes of someone walking through.
      if (act === "empty" && this.wizState === "idle" && this.t > 8) {
        const every = this.o.motionEveryS
        if (Math.floor(this.t) % every === 0 && this.t >= this.scriptedUntil)
          this.scriptedUntil = this.t + 7
      }
    } else {
      this.motion = false
    }

    return this.snapshot()
  }

  private advanceWizard() {
    if (this.wizState === "countdown" && this.t >= this.wizEnd) {
      this.wizState = "recording"
      this.wizEnd = this.t + this.o.recordS
    } else if (this.wizState === "recording" && this.t >= this.wizEnd) {
      if (this.wizStage < 4) {
        this.wizStage += 1
        this.wizState = "waiting"
      } else {
        this.finishWizard()
      }
    }
  }

  private finishWizard() {
    const q = this.o.quality
    const poor = q === "poor"
    const ceil = poor ? 4.8 : 2.4
    const thr = Math.round(ceil * 1.15 * 100) / 100
    this.mean = poor ? 1.1 : 0.9
    this.sigma = poor ? 0.84 : 0.35
    this.max = ceil
    this.thr = thr
    this.res = {
      quality: q,
      mean: this.mean,
      sigma: this.sigma,
      ceil,
      thr,
      mov: poor ? 0 : q === "fair" ? 0.45 : 0.87,
      still: 0,
      stillOk: 0,
      hold: poor ? 2 : 0,
    }
    this.phase = "running"
    this.source = "wizard"
    this.saved = 1
    this.wizState = "done"
  }

  private snapshot(): DeviceState {
    const running = this.phase === "running"
    const presence =
      running && (this.motion || this.t - this.lastMotionT < this.presenceHoldS)
    const left =
      this.wizState === "countdown" || this.wizState === "recording"
        ? Math.max(0, Math.ceil(this.wizEnd - this.t))
        : 0
    return {
      v: 1,
      fw: "demo",
      net: "Home WiFi",
      host: "ripple-demo01",
      ms: Math.round(this.t * 1000),
      rssi: this.rssi,
      ok: 1,
      var: round(this.variance),
      peak: round(this.peak),
      thr: running ? this.thr : 0,
      mean: this.mean,
      sigma: this.sigma,
      max: this.max,
      score: running ? round(Math.max(0, (this.variance - this.mean) / (this.thr - this.mean))) : 0,
      phase: this.phase,
      cal: round(this.cal),
      motion: running && this.motion ? 1 : 0,
      presence: presence ? 1 : 0,
      source: this.source,
      saved: this.saved,
      heap: 36000 + Math.round(this.rng() * 2500),
      wiz: {
        state: this.wizState,
        stage: this.wizStage,
        name: STAGE_NAMES[this.wizStage - 1],
        left,
        total: this.wizState === "countdown" ? this.o.countdownS : this.o.recordS,
      },
      ...(this.res ? { res: this.res } : {}),
    }
  }
}

function round(x: number) {
  return Math.round(x * 100) / 100
}

export function createDemoTransport(options?: DemoOptions): Transport {
  const device = new DemoDevice(options)
  let timer: ReturnType<typeof setInterval> | undefined
  let stateCb: (s: DeviceState) => void = () => {}
  let statusCb: (s: ConnectionStatus) => void = () => {}
  return {
    start() {
      statusCb("open")
      timer = setInterval(() => stateCb(device.tick(0.2)), 200)
    },
    stop() {
      clearInterval(timer)
      statusCb("closed")
    },
    send(command) {
      device.handle(command)
      return true
    },
    onState(cb) {
      stateCb = cb
    },
    onStatus(cb) {
      statusCb = cb
    },
  }
}
