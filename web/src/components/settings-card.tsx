import { useState } from "react"

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
import { Badge } from "@/components/ui/badge"
import { Button } from "@/components/ui/button"
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card"
import { Separator } from "@/components/ui/separator"
import { Slider } from "@/components/ui/slider"
import {
  clampThreshold,
  describeSensitivity,
  thresholdRange,
} from "@/lib/threshold"
import type { Command, DeviceState } from "@/lib/types"

type Run = (command: Command, ack: (s: DeviceState) => boolean) => Promise<void>

export function SettingsCard({
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
  const [draft, setDraft] = useState<number | null>(null)
  const running = state?.phase === "running"
  const enabled = connected && running

  const range = thresholdRange(state ?? { mean: 0, max: 0, thr: 0 })
  const value = clampThreshold(draft ?? state?.thr ?? range.min, range)

  const commit = async (v: number) => {
    try {
      await run({ cmd: "set_threshold", value: v }, (s) => Math.abs(s.thr - v) < 0.06)
    } catch (e) {
      onError(e instanceof Error ? e.message : "Command failed.")
    } finally {
      setDraft(null)
    }
  }

  const reset = async () => {
    try {
      await run({ cmd: "reset_calibration" }, (s) => s.phase === "calibrating")
    } catch (e) {
      onError(e instanceof Error ? e.message : "Command failed.")
    }
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle>Sensitivity</CardTitle>
        <CardDescription>
          Motion starts when the fluctuation rises above this threshold.
        </CardDescription>
      </CardHeader>
      <CardContent className="flex flex-col gap-5">
        <div className="flex items-baseline justify-between gap-3">
          <span className="tabular text-3xl font-semibold tracking-tight">
            {running ? value.toFixed(1) : "—"}
          </span>
          {running && state && (
            <Badge variant="secondary">
              {describeSensitivity(value, state.max)} sensitivity
            </Badge>
          )}
        </div>

        <div className="flex flex-col gap-2">
          <Slider
            min={range.min}
            max={range.max}
            step={range.step}
            value={[value]}
            disabled={!enabled}
            onValueChange={([v]) => setDraft(v)}
            onValueCommit={([v]) => commit(v)}
            aria-label="Motion threshold"
          />
          <div className="flex justify-between text-xs text-muted-foreground">
            <span>More sensitive</span>
            <span>More robust</span>
          </div>
        </div>

        {running && state && (
          <p className="text-sm text-muted-foreground">
            The empty room's noise peaks at{" "}
            <span className="tabular font-medium text-foreground">
              {state.max.toFixed(1)}
            </span>
            . Staying above it avoids false alarms.
          </p>
        )}

        <Separator />

        <AlertDialog>
          <AlertDialogTrigger asChild>
            <Button variant="destructive" disabled={!connected} className="self-start">
              Reset calibration
            </Button>
          </AlertDialogTrigger>
          <AlertDialogContent>
            <AlertDialogHeader>
              <AlertDialogTitle>Reset the calibration?</AlertDialogTitle>
              <AlertDialogDescription>
                The saved calibration is deleted and a quick calibration starts.
                Keep the room empty for about 45 seconds.
              </AlertDialogDescription>
            </AlertDialogHeader>
            <AlertDialogFooter>
              <AlertDialogCancel>Cancel</AlertDialogCancel>
              <AlertDialogAction onClick={reset}>Reset</AlertDialogAction>
            </AlertDialogFooter>
          </AlertDialogContent>
        </AlertDialog>
      </CardContent>
    </Card>
  )
}
