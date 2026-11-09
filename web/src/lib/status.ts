import { formatAgo } from "@/lib/format"
import type { ConnectionStatus, DeviceState } from "@/lib/types"

export type StatusKind =
  | "offline"
  | "uncalibrated"
  | "calibrating"
  | "motion"
  | "presence"
  | "empty"

export interface StatusView {
  kind: StatusKind
  title: string
  subtitle: string
}

/** What the big status indicator should say. */
export function describeStatus(
  state: DeviceState | null,
  status: ConnectionStatus,
  sinceMotionS: number | null
): StatusView {
  if (!state) {
    return {
      kind: "offline",
      title: status === "connecting" ? "Connecting" : "Device offline",
      subtitle:
        status === "connecting"
          ? "Waiting for the first reading"
          : "Trying to reach the board…",
    }
  }
  if (status !== "open") {
    return {
      kind: "offline",
      title: "Connection lost",
      subtitle: "Reconnecting to the board…",
    }
  }
  if (state.phase === "calibrating") {
    return {
      kind: "calibrating",
      title: "Calibrating",
      subtitle: "Keep the room empty for a moment",
    }
  }
  if (state.phase === "uncalibrated") {
    return {
      kind: "uncalibrated",
      title: "Not calibrated",
      subtitle: "Run the guided calibration to start detecting",
    }
  }
  if (state.motion === 1) {
    return {
      kind: "motion",
      title: "Motion detected",
      subtitle: "The signal is fluctuating above the threshold",
    }
  }
  if (state.presence === 1) {
    return {
      kind: "presence",
      title: "Presence",
      subtitle: `Last motion ${formatAgo(sinceMotionS)}`,
    }
  }
  return {
    kind: "empty",
    title: "Room is empty",
    subtitle:
      sinceMotionS === null
        ? "No motion seen yet"
        : `Last motion ${formatAgo(sinceMotionS)}`,
  }
}
