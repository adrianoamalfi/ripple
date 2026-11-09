// Shapes of the device API (see src/web.h in the firmware).

export type Phase = "uncalibrated" | "calibrating" | "running"
export type WizardState =
  | "idle"
  | "waiting"
  | "countdown"
  | "recording"
  | "done"
  | "failed"
export type Quality = "good" | "fair" | "poor"

export interface WizardInfo {
  state: WizardState
  stage: number // 1..4
  name: string
  left: number // seconds left in countdown / recording
  total?: number // length of the current countdown / recording (newer firmware)
}

export interface CalibrationResult {
  quality: Quality
  mean: number
  sigma: number
  ceil: number // highest empty-room variance
  thr: number
  mov: number // 0..1 share of walking seconds above threshold
  still: number // 0..1 same for sitting still
  stillOk: number // 0 | 1
  hold: number // false-alarm seconds in the second empty step
}

export interface DeviceState {
  v: number
  fw: string
  ms: number // device uptime
  rssi: number
  ok: number // 1 when the WiFi link is valid
  var: number
  peak: number // highest window variance since the previous message
  thr: number
  mean: number
  sigma: number
  max: number
  score: number
  phase: Phase
  cal: number // quick calibration progress 0..1
  motion: number
  presence: number
  source: string // "wizard" | "quick" | "manual" | "none"
  saved: number
  heap: number
  wiz: WizardInfo
  res?: CalibrationResult
  mqtt?: MqttState // newer firmware
  net?: string // WiFi network the board is on (newer firmware)
  host?: string // mDNS name without .local, e.g. ripple-4f7ddc
}

export type MqttState = "disabled" | "connecting" | "connected" | "failed"

/** GET /api/mqtt. The password is write-only: only `hasPass` is returned. */
export interface MqttConfig {
  enabled: number
  host: string
  port: number
  user: string
  hasPass: number
  prefix: string
  state: MqttState
  error: string
  id: string // Home Assistant device id, e.g. ripple_4f7ddc
  base: string // topic root, e.g. ripple/ripple_4f7ddc
}

/** POST /api/mqtt: any subset. A missing `pass` keeps the stored password. */
export interface MqttUpdate {
  enabled?: boolean
  host?: string
  port?: number
  user?: string
  pass?: string
  prefix?: string
}

export type Command =
  | { cmd: "wizard_start" }
  | { cmd: "wizard_ready" }
  | { cmd: "wizard_cancel" }
  | { cmd: "quick_calibrate" }
  | { cmd: "reset_calibration" }
  | { cmd: "set_threshold"; value: number }

export type ConnectionStatus = "connecting" | "open" | "stalled" | "closed"

export interface Transport {
  start(): void
  stop(): void
  send(command: Command): boolean
  onState(cb: (state: DeviceState) => void): void
  onStatus(cb: (status: ConnectionStatus) => void): void
}
