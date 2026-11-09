import {
  ActivityIcon,
  LoaderCircleIcon,
  RadarIcon,
  SlidersHorizontalIcon,
  UserRoundIcon,
  WifiOffIcon,
  type LucideIcon,
} from "lucide-react"

import { SignalBars } from "@/components/signal-bars"
import { Badge } from "@/components/ui/badge"
import { Card, CardContent } from "@/components/ui/card"
import { Progress } from "@/components/ui/progress"
import { Separator } from "@/components/ui/separator"
import { Skeleton } from "@/components/ui/skeleton"
import { signalQuality } from "@/lib/format"
import { describeStatus, type StatusKind } from "@/lib/status"
import { describeSensitivity } from "@/lib/threshold"
import type { ConnectionStatus, DeviceState } from "@/lib/types"
import { cn } from "@/lib/utils"

const ICON: Record<StatusKind, LucideIcon> = {
  offline: WifiOffIcon,
  uncalibrated: SlidersHorizontalIcon,
  calibrating: LoaderCircleIcon,
  motion: ActivityIcon,
  presence: UserRoundIcon,
  empty: RadarIcon,
}

const TONE: Record<StatusKind, string> = {
  offline: "bg-muted text-muted-foreground ring-border",
  uncalibrated: "bg-warn/15 text-warn ring-warn/30",
  calibrating: "bg-primary/10 text-primary ring-primary/25",
  motion: "bg-motion/15 text-motion ring-motion/40",
  presence: "bg-primary/12 text-primary ring-primary/35",
  empty: "bg-primary/10 text-primary ring-primary/25",
}

/** Concentric rings; they ripple outwards while something is happening. */
function Orb({ kind }: { kind: StatusKind }) {
  const Icon = ICON[kind]
  const ripple =
    kind === "motion"
      ? "animate-ripple border-motion"
      : kind === "presence"
        ? "animate-ripple-slow border-primary"
        : null
  return (
    <div className="relative grid size-44 place-items-center">
      <span className="absolute inset-0 rounded-full border border-border/60" />
      <span className="absolute inset-5 rounded-full border border-border/80" />
      {ripple &&
        [0, 1, 2].map((i) => (
          <span
            key={i}
            className={cn("absolute inset-6 rounded-full border-2", ripple)}
            style={{ animationDelay: `${i * (kind === "motion" ? 0.85 : 1.4)}s` }}
          />
        ))}
      <div
        className={cn(
          "relative grid size-24 place-items-center rounded-full ring-1 transition-colors duration-500",
          TONE[kind]
        )}
      >
        <Icon
          className={cn("size-10", kind === "calibrating" && "animate-spin")}
          strokeWidth={1.75}
        />
      </div>
    </div>
  )
}

function Stat({
  label,
  value,
  unit,
  hint,
  children,
}: {
  label: string
  value: string
  unit?: string
  hint?: React.ReactNode
  children?: React.ReactNode
}) {
  return (
    <div className="flex min-w-0 flex-col gap-1">
      <dt className="text-xs font-medium tracking-wide text-muted-foreground uppercase">
        {label}
      </dt>
      <dd className="flex flex-col gap-1">
        <span className="tabular text-2xl font-semibold tracking-tight">
          {value}
          {unit && (
            <span className="ml-1 text-sm font-normal text-muted-foreground">
              {unit}
            </span>
          )}
        </span>
        {children}
        {hint && <span className="text-xs text-muted-foreground">{hint}</span>}
      </dd>
    </div>
  )
}

export function StatusHero({
  state,
  status,
  sinceMotionS,
}: {
  state: DeviceState | null
  status: ConnectionStatus
  sinceMotionS: number | null
}) {
  const view = describeStatus(state, status, sinceMotionS)
  const running = state?.phase === "running"
  const q = signalQuality(state && state.ok ? state.rssi : null)
  const activity = running && state.thr > 0 ? state.var / state.thr : 0

  return (
    <Card>
      <CardContent className="flex flex-col items-center gap-8 md:flex-row md:gap-10">
        <div className="flex flex-col items-center gap-5 text-center md:w-72 md:shrink-0">
          <Orb kind={view.kind} />
          <div className="flex flex-col gap-1">
            <h2
              className="text-2xl font-semibold tracking-tight"
              aria-live="polite"
            >
              {view.title}
            </h2>
            <p className="text-sm text-muted-foreground">{view.subtitle}</p>
          </div>
          <div className="flex gap-2">
            <Badge variant={running && state.motion ? "default" : "outline"}
              className={cn(running && state.motion && "bg-motion text-motion-foreground")}>
              Motion
            </Badge>
            <Badge variant={running && state.presence ? "default" : "outline"}>
              Presence
            </Badge>
          </div>
        </div>

        <Separator orientation="vertical" className="hidden self-stretch md:block" />
        <Separator className="md:hidden" />

        {state ? (
          <dl className="grid w-full flex-1 grid-cols-2 gap-x-6 gap-y-6">
            <Stat
              label="Signal"
              value={state.ok ? String(state.rssi) : "—"}
              unit={state.ok ? "dBm" : undefined}
              hint={
                <span className="inline-flex items-center gap-2">
                  <SignalBars bars={q.bars} />
                  {q.label}
                </span>
              }
            />
            <Stat
              label="Fluctuation"
              value={state.var.toFixed(2)}
              hint="Window variance, dB²"
            />
            <Stat
              label="Threshold"
              value={running ? state.thr.toFixed(2) : "—"}
              hint={
                running
                  ? `Sensitivity: ${describeSensitivity(state.thr, state.max)}`
                  : "Set by calibration"
              }
            />
            <Stat
              label="Activity"
              value={running ? `${Math.round(activity * 100)}%` : "—"}
              hint="Of the motion threshold"
            >
              <Progress
                value={Math.min(100, activity * 100)}
                className={cn(activity >= 1 && "[&>div]:bg-motion")}
              />
            </Stat>
          </dl>
        ) : (
          <div className="grid w-full flex-1 grid-cols-2 gap-6">
            {[0, 1, 2, 3].map((i) => (
              <div key={i} className="flex flex-col gap-2">
                <Skeleton className="h-3 w-16" />
                <Skeleton className="h-7 w-24" />
                <Skeleton className="h-3 w-28" />
              </div>
            ))}
          </div>
        )}
      </CardContent>
    </Card>
  )
}
