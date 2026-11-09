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
import { Button } from "@/components/ui/button"
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card"
import { Separator } from "@/components/ui/separator"
import { Skeleton } from "@/components/ui/skeleton"
import { formatCalibrationSource, formatDuration } from "@/lib/format"
import type { DeviceState } from "@/lib/types"
import { setupNetworkName } from "@/lib/wifi-api"

function Row({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex items-baseline justify-between gap-4 py-2 not-last:border-b first:pt-0 last:pb-0">
      <dt className="text-sm text-muted-foreground">{label}</dt>
      <dd className="tabular truncate text-sm font-medium">{value}</dd>
    </div>
  )
}

export function DeviceCard({
  state,
  address,
  connected,
  onForgetWifi,
}: {
  state: DeviceState | null
  address: string
  connected: boolean
  onForgetWifi: () => void
}) {
  return (
    <Card>
      <CardHeader>
        <CardTitle>Device</CardTitle>
        <CardDescription>The board running Ripple.</CardDescription>
      </CardHeader>
      <CardContent>
        {state ? (
          <>
            <dl className="flex flex-col">
              <Row
                label="Address"
                value={state.host ? `${state.host}.local` : address}
              />
              {state.net && <Row label="WiFi network" value={state.net} />}
              <Row label="Firmware" value={`v${state.fw}`} />
              <Row label="Uptime" value={formatDuration(state.ms / 1000)} />
              <Row
                label="Free memory"
                value={`${(state.heap / 1024).toFixed(1)} KB`}
              />
              {state.mqtt && (
                <Row
                  label="MQTT"
                  value={
                    state.mqtt === "connected"
                      ? "Connected"
                      : state.mqtt === "disabled"
                        ? "Off"
                        : state.mqtt === "failed"
                          ? "Failed"
                          : "Connecting"
                  }
                />
              )}
              <Row
                label="Calibration"
                value={
                  state.phase === "uncalibrated"
                    ? "None"
                    : `${formatCalibrationSource(state.source)}${state.saved ? "" : " (not saved)"}`
                }
              />
            </dl>
            <Separator className="my-4" />
            <AlertDialog>
              <AlertDialogTrigger asChild>
                <Button
                  variant="ghost"
                  size="sm"
                  disabled={!connected}
                  className="text-muted-foreground"
                >
                  Forget this WiFi network
                </Button>
              </AlertDialogTrigger>
              <AlertDialogContent>
                <AlertDialogHeader>
                  <AlertDialogTitle>Forget this WiFi network?</AlertDialogTitle>
                  <AlertDialogDescription>
                    The board restarts and opens its own setup network, called{" "}
                    <span className="font-medium">
                      {setupNetworkName(state.host)}
                    </span>
                    . Join it from your phone to choose a new WiFi. Motion
                    sensing is paused until the board is back online.
                  </AlertDialogDescription>
                </AlertDialogHeader>
                <AlertDialogFooter>
                  <AlertDialogCancel>Cancel</AlertDialogCancel>
                  <AlertDialogAction onClick={onForgetWifi}>
                    Forget and restart
                  </AlertDialogAction>
                </AlertDialogFooter>
              </AlertDialogContent>
            </AlertDialog>
          </>
        ) : (
          <div className="flex flex-col gap-3">
            {[0, 1, 2, 3].map((i) => (
              <Skeleton key={i} className="h-5 w-full" />
            ))}
          </div>
        )}
      </CardContent>
    </Card>
  )
}
