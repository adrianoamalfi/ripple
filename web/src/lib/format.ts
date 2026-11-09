export function formatDuration(totalSeconds: number): string {
  const s = Math.max(0, Math.floor(totalSeconds))
  if (s < 60) return `${s}s`
  const m = Math.floor(s / 60)
  if (m < 60) return `${m}m ${String(s % 60).padStart(2, "0")}s`
  const h = Math.floor(m / 60)
  if (h < 48) return `${h}h ${String(m % 60).padStart(2, "0")}m`
  return `${Math.floor(h / 24)}d ${h % 24}h`
}

export function formatAgo(seconds: number | null): string {
  if (seconds === null) return "not seen yet"
  if (seconds < 2) return "just now"
  return `${formatDuration(seconds)} ago`
}

export interface SignalQuality {
  label: string
  bars: 0 | 1 | 2 | 3 | 4
}

/** Rough WiFi signal quality from RSSI. */
export function signalQuality(rssi: number | null): SignalQuality {
  if (rssi === null) return { label: "No link", bars: 0 }
  if (rssi >= -55) return { label: "Very strong", bars: 4 }
  if (rssi >= -65) return { label: "Strong", bars: 3 }
  if (rssi >= -75) return { label: "Fair", bars: 2 }
  return { label: "Weak", bars: 1 }
}

export function formatCalibrationSource(source: string): string {
  switch (source) {
    case "wizard":
      return "Guided calibration"
    case "quick":
      return "Quick calibration"
    case "manual":
      return "Manual threshold"
    default:
      return "Not calibrated"
  }
}

export function pct(x: number): string {
  return `${Math.round(Math.max(0, Math.min(1, x)) * 100)}%`
}
