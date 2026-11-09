import { useState } from "react"
import {
  ArmchairIcon,
  CheckIcon,
  DoorClosedIcon,
  DoorOpenIcon,
  FootprintsIcon,
  PlayIcon,
  RotateCcwIcon,
  TriangleAlertIcon,
  type LucideIcon,
} from "lucide-react"

import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
  AlertDialogTrigger,
} from "@/components/ui/alert-dialog"
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert"
import { Badge } from "@/components/ui/badge"
import { Button } from "@/components/ui/button"
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card"
import { Progress } from "@/components/ui/progress"
import { Separator } from "@/components/ui/separator"
import { formatCalibrationSource, pct } from "@/lib/format"
import type { CalibrationResult, Command, DeviceState } from "@/lib/types"
import { cn } from "@/lib/utils"

type Run = (command: Command, ack: (s: DeviceState) => boolean) => Promise<void>

const STEPS: Array<{
  icon: LucideIcon
  short: string
  title: string
  hint: string
}> = [
  {
    icon: DoorOpenIcon,
    short: "Empty room",
    title: "Leave the room",
    hint: "Go out and close the door. The board learns what an empty room looks like.",
  },
  {
    icon: FootprintsIcon,
    short: "Walk",
    title: "Walk around the room",
    hint: "Walk back and forth, passing between the board and the router.",
  },
  {
    icon: ArmchairIcon,
    short: "Sit still",
    title: "Sit still in the room",
    hint: "Sit down and stay as still as you can.",
  },
  {
    icon: DoorClosedIcon,
    short: "Empty again",
    title: "Leave the room again",
    hint: "Go out once more. This checks that the empty-room noise is stable.",
  },
]

function Stepper({ current }: { current: number }) {
  return (
    <ol className="grid grid-cols-4 gap-2" aria-label="Calibration steps">
      {STEPS.map((step, i) => {
        const n = i + 1
        const done = n < current
        const active = n === current
        const Icon = done ? CheckIcon : step.icon
        return (
          <li
            key={step.short}
            className="flex flex-col items-center gap-2 text-center"
            aria-current={active ? "step" : undefined}
          >
            <span
              className={cn(
                "grid size-10 place-items-center rounded-full ring-1 transition-colors",
                done && "bg-primary text-primary-foreground ring-primary",
                active && "bg-primary/10 text-primary ring-2 ring-primary",
                !done && !active && "bg-muted text-muted-foreground ring-border"
              )}
            >
              <Icon className="size-4.5" />
            </span>
            <span
              className={cn(
                "text-xs",
                active ? "font-medium text-foreground" : "text-muted-foreground"
              )}
            >
              {step.short}
            </span>
          </li>
        )
      })}
    </ol>
  )
}

const QUALITY: Record<
  CalibrationResult["quality"],
  { label: string; headline: string; variant: "default" | "secondary" | "destructive" }
> = {
  good: {
    label: "Good",
    headline: "Walking is clearly visible in the signal",
    variant: "default",
  },
  fair: {
    label: "Fair",
    headline: "Detection works, but misses part of the movement",
    variant: "secondary",
  },
  poor: {
    label: "Poor",
    headline: "Walking can't be told apart from an empty room",
    variant: "destructive",
  },
}

function Coverage({ label, value }: { label: string; value: number }) {
  return (
    <div className="flex flex-col gap-1.5">
      <div className="flex items-baseline justify-between text-sm">
        <span>{label}</span>
        <span className="tabular font-medium">{pct(value)}</span>
      </div>
      <Progress value={value * 100} />
    </div>
  )
}

function Metric({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex flex-col gap-0.5">
      <dt className="text-xs text-muted-foreground">{label}</dt>
      <dd className="tabular text-sm font-medium">{value}</dd>
    </div>
  )
}

function ResultPanel({ res, threshold }: { res: CalibrationResult; threshold: number }) {
  const q = QUALITY[res.quality]
  return (
    <div className="flex flex-col gap-5">
      <div className="flex flex-wrap items-center gap-3">
        <Badge variant={q.variant}>{q.label}</Badge>
        <p className="text-sm font-medium">{q.headline}</p>
      </div>

      <div className="grid gap-4 sm:grid-cols-2">
        <Coverage label="Walking detected" value={res.mov} />
        <Coverage label="Sitting still detected" value={res.still} />
      </div>

      <dl className="grid grid-cols-2 gap-4 sm:grid-cols-4">
        <Metric label="Threshold" value={threshold.toFixed(2)} />
        <Metric label="Empty-room noise" value={res.mean.toFixed(2)} />
        <Metric label="Noise peak" value={res.ceil.toFixed(2)} />
        <Metric label="False seconds" value={String(res.hold)} />
      </dl>

      {res.quality === "poor" && (
        <Alert variant="destructive">
          <TriangleAlertIcon />
          <AlertTitle>Try a different position</AlertTitle>
          <AlertDescription>
            At this spot the signal is too stable to see you walking. Move the
            board a few metres away from the router, so the signal is weaker,
            then calibrate again.
          </AlertDescription>
        </Alert>
      )}
      {res.quality === "fair" && (
        <Alert>
          <TriangleAlertIcon />
          <AlertTitle>A different position may help</AlertTitle>
          <AlertDescription>
            Some movement is missed. Try placing the board so people cross the
            line between it and the router.
          </AlertDescription>
        </Alert>
      )}
      {!res.stillOk && (
        <p className="text-sm text-muted-foreground">
          A person sitting perfectly still can't be detected with the WiFi
          signal alone. Presence stays on for a while after the last movement.
        </p>
      )}
    </div>
  )
}

export function CalibrationCard({
  state,
  connected,
  run,
  onError,
}: {
  state: DeviceState | null
  connected: boolean
  run: Run
  onError: (message: string) => void
}) {
  const [pending, setPending] = useState(false)

  const act = async (command: Command, ack: (s: DeviceState) => boolean) => {
    setPending(true)
    try {
      await run(command, ack)
    } catch (e) {
      onError(e instanceof Error ? e.message : "Command failed.")
    } finally {
      setPending(false)
    }
  }

  const wiz = state?.wiz
  const inWizard =
    wiz?.state === "waiting" || wiz?.state === "countdown" || wiz?.state === "recording"
  const quick = state?.phase === "calibrating" && !inWizard
  const disabled = !connected || pending

  const startWizard = () =>
    act({ cmd: "wizard_start" }, (s) => s.wiz.state === "waiting")
  const cancel = () => act({ cmd: "wizard_cancel" }, (s) => s.wiz.state === "idle")

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex flex-wrap items-center gap-2">
          Calibration
          {state && state.phase !== "uncalibrated" && (
            <Badge variant="outline">
              {formatCalibrationSource(state.source)}
              {state.saved ? " · saved" : ""}
            </Badge>
          )}
        </CardTitle>
        <CardDescription>
          Teach the board what your room looks like. Do it once the board is in
          its final position.
        </CardDescription>
      </CardHeader>

      <CardContent className="flex flex-col gap-6">
        {inWizard && wiz && (
          <>
            <Stepper current={wiz.stage} />
            <Separator />
          </>
        )}

        {inWizard && wiz && wiz.state === "waiting" && (
          <div className="flex flex-col gap-4">
            <div className="flex flex-col gap-1">
              <h3 className="text-lg font-semibold tracking-tight">
                {STEPS[wiz.stage - 1]?.title}
              </h3>
              <p className="text-sm text-muted-foreground">
                {STEPS[wiz.stage - 1]?.hint}
              </p>
            </div>
            <div className="flex flex-wrap gap-2">
              <Button
                size="lg"
                disabled={disabled}
                onClick={() =>
                  act({ cmd: "wizard_ready" }, (s) => s.wiz.state !== "waiting")
                }
              >
                <PlayIcon data-icon="inline-start" />
                I'm in position, start
              </Button>
              <Button size="lg" variant="ghost" disabled={disabled} onClick={cancel}>
                Cancel
              </Button>
            </div>
          </div>
        )}

        {inWizard && wiz && wiz.state === "countdown" && (
          <div className="flex flex-col gap-4">
            <div className="flex items-center gap-5">
              <span className="tabular grid size-20 shrink-0 place-items-center rounded-full bg-primary/10 text-4xl font-semibold text-primary ring-1 ring-primary/30">
                {wiz.left}
              </span>
              <div className="flex flex-col gap-1">
                <h3 className="text-lg font-semibold tracking-tight">
                  {STEPS[wiz.stage - 1]?.title}
                </h3>
                <p className="text-sm text-muted-foreground">
                  Get in position. Recording starts in {wiz.left}s.
                </p>
              </div>
            </div>
            <div>
              <Button variant="ghost" disabled={disabled} onClick={cancel}>
                Cancel
              </Button>
            </div>
          </div>
        )}

        {inWizard && wiz && wiz.state === "recording" && (
          <div className="flex flex-col gap-4">
            <div className="flex flex-col gap-1">
              <div className="flex items-center gap-2">
                <span className="relative flex size-2.5">
                  <span className="absolute inline-flex size-full animate-ping rounded-full bg-destructive opacity-60" />
                  <span className="relative inline-flex size-2.5 rounded-full bg-destructive" />
                </span>
                <h3 className="text-lg font-semibold tracking-tight">
                  Recording · {wiz.left}s left
                </h3>
              </div>
              <p className="text-sm text-muted-foreground">{STEPS[wiz.stage - 1]?.hint}</p>
            </div>
            <Progress value={100 - (wiz.left / (wiz.total || 30)) * 100} />
            <div>
              <Button variant="ghost" disabled={disabled} onClick={cancel}>
                Cancel
              </Button>
            </div>
          </div>
        )}

        {quick && state && (
          <div className="flex flex-col gap-3">
            <div className="flex flex-col gap-1">
              <h3 className="text-lg font-semibold tracking-tight">Quick calibration</h3>
              <p className="text-sm text-muted-foreground">
                Keep the room empty while the board listens to the signal.
              </p>
            </div>
            <Progress value={state.cal * 100} />
          </div>
        )}

        {!inWizard && !quick && wiz?.state === "failed" && (
          <Alert variant="destructive">
            <TriangleAlertIcon />
            <AlertTitle>Calibration failed</AlertTitle>
            <AlertDescription>
              Not enough readings were received. Check the WiFi link and try
              again.
            </AlertDescription>
          </Alert>
        )}

        {!inWizard && !quick && state?.res && state.phase === "running" && (
          <ResultPanel res={state.res} threshold={state.thr} />
        )}

        {!inWizard && !quick && state && !state.res && wiz?.state !== "failed" && (
          <p className="text-sm text-muted-foreground">
            {state.phase === "uncalibrated"
              ? "The board has not been calibrated yet. "
              : "No guided calibration has been run yet. "}
            The guided calibration takes about two minutes and tunes the
            threshold from your own data, so detection fits your room.
          </p>
        )}

        {!inWizard && !quick && (
          <div className="flex flex-wrap gap-2">
            <Button size="lg" disabled={disabled || !state} onClick={startWizard}>
              {state?.res ? (
                <RotateCcwIcon data-icon="inline-start" />
              ) : (
                <PlayIcon data-icon="inline-start" />
              )}
              {state?.res ? "Calibrate again" : "Start guided calibration"}
            </Button>
            <AlertDialog>
              <AlertDialogTrigger asChild>
                <Button size="lg" variant="outline" disabled={disabled || !state}>
                  Quick calibration
                </Button>
              </AlertDialogTrigger>
              <AlertDialogContent>
                <AlertDialogHeader>
                  <AlertDialogTitle>Start a quick calibration?</AlertDialogTitle>
                  <AlertDialogDescription>
                    The room must be empty for about 45 seconds. It is faster
                    than the guided calibration but cannot check how well
                    walking is detected.
                  </AlertDialogDescription>
                </AlertDialogHeader>
                <AlertDialogFooter>
                  <AlertDialogCancel>Cancel</AlertDialogCancel>
                  <AlertDialogAction
                    onClick={() =>
                      act({ cmd: "quick_calibrate" }, (s) => s.phase === "calibrating")
                    }
                  >
                    Start
                  </AlertDialogAction>
                </AlertDialogFooter>
              </AlertDialogContent>
            </AlertDialog>
          </div>
        )}
      </CardContent>
    </Card>
  )
}
