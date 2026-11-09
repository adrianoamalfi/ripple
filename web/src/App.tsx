import { useEffect, useMemo, useState } from "react"
import { InfoIcon, TriangleAlertIcon } from "lucide-react"

import { CalibrationCard } from "@/components/calibration-card"
import { ConnectionBadge } from "@/components/connection-badge"
import { DeviceCard } from "@/components/device-card"
import { Logo } from "@/components/logo"
import { MqttCard } from "@/components/mqtt-card"
import { SettingsCard } from "@/components/settings-card"
import { SignalChart } from "@/components/signal-chart"
import { StatusHero } from "@/components/status-hero"
import { ThemeToggle } from "@/components/theme-toggle"
import { Alert, AlertDescription } from "@/components/ui/alert"
import { useRipple } from "@/hooks/use-ripple"
import { createDemoTransport } from "@/lib/demo"
import { secondsSinceMotion } from "@/lib/history"
import { createDemoMqttApi, createHttpMqttApi } from "@/lib/mqtt-api"
import { createSocketTransport } from "@/lib/transport"
import {
  createDemoWifiApi,
  createHttpWifiApi,
  setupNetworkName,
} from "@/lib/wifi-api"
import type { Transport } from "@/lib/types"

const params = new URLSearchParams(window.location.search)
const deviceHost: string = import.meta.env.VITE_DEVICE || window.location.host
// `?demo` (or a build with VITE_DEMO=1, used for the project site) runs against a
// simulated board. `?demo=poor` simulates a bad calibration, `?demo=fast` shortens
// the calibration wizard; combine them as `?demo=poor,fast`.
const demoFlags = (params.get("demo") ?? "").split(",")
const demo =
  params.has("demo") ||
  import.meta.env.VITE_DEMO === "1" ||
  (import.meta.env.DEV && !import.meta.env.VITE_DEVICE)

const mqttApi = demo ? createDemoMqttApi() : createHttpMqttApi()
const wifiApi = demo ? createDemoWifiApi() : createHttpWifiApi()

function createTransport(): Transport {
  if (demo) {
    const fast = demoFlags.includes("fast")
    return createDemoTransport({
      quality: demoFlags.includes("poor") ? "poor" : "good",
      ...(fast ? { countdownS: 1, recordS: 3, motionEveryS: 12 } : {}),
    })
  }
  const scheme = window.location.protocol === "https:" ? "wss" : "ws"
  return createSocketTransport(`${scheme}://${deviceHost}/ws`)
}

export function App() {
  const { state, status, run, getHistory, nowMs } = useRipple(createTransport)
  const [error, setError] = useState<string | null>(null)
  const [notice, setNotice] = useState<string | null>(null)

  useEffect(() => {
    if (!error) return
    const id = setTimeout(() => setError(null), 6000)
    return () => clearTimeout(id)
  }, [error])

  useEffect(() => {
    if (!notice) return
    const id = setTimeout(() => setNotice(null), 12000)
    return () => clearTimeout(id)
  }, [notice])

  const forgetWifi = async () => {
    try {
      await wifiApi.forget()
      setNotice(
        demo
          ? "Demo: a real board would restart now and open its setup network."
          : `The board is restarting. When it is back, join the WiFi network "${setupNetworkName(state?.host)}" from your phone to choose a new network.`
      )
    } catch (e) {
      setError(e instanceof Error ? e.message : "Could not reach the board.")
    }
  }

  const connected = status === "open" && state !== null
  const sinceMotion = useMemo(
    () => secondsSinceMotion(getHistory(), nowMs),
    [getHistory, nowMs]
  )

  return (
    <div className="mx-auto flex min-h-svh max-w-5xl flex-col gap-5 px-4 py-5 sm:px-6 sm:py-8">
      <header className="flex items-center justify-between gap-3">
        <div className="flex items-center gap-3">
          <Logo className="size-8 text-primary" />
          <div className="flex flex-col">
            <h1 className="text-lg leading-tight font-semibold tracking-tight">
              Ripple
            </h1>
            <p className="text-xs text-muted-foreground">
              Motion sensing with WiFi alone
            </p>
          </div>
        </div>
        <div className="flex items-center gap-1">
          <ConnectionBadge status={status} demo={demo} />
          <ThemeToggle />
        </div>
      </header>

      {error && (
        <Alert variant="destructive" role="alert">
          <TriangleAlertIcon />
          <AlertDescription>{error}</AlertDescription>
        </Alert>
      )}

      {demo && (
        <p
          data-demo-banner
          className="rounded-lg bg-primary/10 px-3 py-2 text-center text-sm text-primary"
        >
          You are looking at a simulated board: everything works, nothing is
          real.{" "}
          <a
            className="font-medium underline underline-offset-2"
            href={
              import.meta.env.VITE_SITE_URL ??
              "https://github.com/adrianoamalfi/ripple"
            }
          >
            Get Ripple
          </a>
        </p>
      )}

      {notice && (
        <Alert role="status">
          <InfoIcon />
          <AlertDescription>{notice}</AlertDescription>
        </Alert>
      )}

      <StatusHero state={state} status={status} sinceMotionS={sinceMotion} />

      <SignalChart getHistory={getHistory} nowMs={nowMs} />

      <div className="grid gap-5 lg:grid-cols-5">
        <div className="flex flex-col gap-5 lg:col-span-3">
          <CalibrationCard
            state={state}
            connected={connected}
            run={run}
            onError={setError}
          />
          <MqttCard
            api={mqttApi}
            liveState={state?.mqtt}
            connected={connected}
          />
        </div>
        <div className="flex flex-col gap-5 lg:col-span-2">
          <SettingsCard
            state={state}
            connected={connected}
            run={run}
            onError={setError}
          />
          <DeviceCard
            state={state}
            address={demo ? "demo" : deviceHost}
            connected={connected}
            onForgetWifi={forgetWifi}
          />
        </div>
      </div>

      <footer className="pb-2 text-center text-xs text-muted-foreground">
        Ripple is open source (MIT). Presence from a still person is not
        detectable with WiFi RSSI alone.
      </footer>
    </div>
  )
}

export default App
