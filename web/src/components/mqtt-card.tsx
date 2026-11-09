import { useState } from "react"
import { CircleCheckIcon, TriangleAlertIcon } from "lucide-react"

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
import {
  Field,
  FieldContent,
  FieldDescription,
  FieldGroup,
  FieldLabel,
} from "@/components/ui/field"
import { Input } from "@/components/ui/input"
import { Skeleton } from "@/components/ui/skeleton"
import { Spinner } from "@/components/ui/spinner"
import { Switch } from "@/components/ui/switch"
import { useMqtt } from "@/hooks/use-mqtt"
import type { MqttApi } from "@/lib/mqtt-api"
import { validateMqtt } from "@/lib/mqtt-validate"
import type { MqttConfig, MqttState, MqttUpdate } from "@/lib/types"

const ENTITIES = [
  "Motion",
  "Presence",
  "Signal strength",
  "Signal fluctuation",
  "Motion threshold",
  "Quick calibration",
]

interface Draft {
  enabled: boolean
  host: string
  port: string
  user: string
  pass: string // only what the user typed: the stored password is never sent back
  clearPass: boolean
  prefix: string
}

function toDraft(c: MqttConfig): Draft {
  return {
    enabled: !!c.enabled,
    host: c.host,
    port: String(c.port),
    user: c.user,
    pass: "",
    clearPass: false,
    prefix: c.prefix,
  }
}

/** Only what changed, so an untouched password is never overwritten. */
function diff(c: MqttConfig, d: Draft): MqttUpdate {
  const u: MqttUpdate = {}
  if (d.enabled !== !!c.enabled) u.enabled = d.enabled
  if (d.host.trim() !== c.host) u.host = d.host.trim()
  if (Number(d.port) !== c.port) u.port = Number(d.port)
  if (d.user !== c.user) u.user = d.user
  if (d.pass) u.pass = d.pass
  else if (d.clearPass) u.pass = ""
  if (d.prefix.trim() !== c.prefix) u.prefix = d.prefix.trim()
  return u
}

function StatusBadge({ state, error }: { state: MqttState; error: string }) {
  switch (state) {
    case "connected":
      return (
        <Badge className="gap-1.5 bg-ok/15 text-ok">
          <span className="size-1.5 animate-breathe rounded-full bg-ok" />
          Connected
        </Badge>
      )
    case "connecting":
      return (
        <Badge variant="outline">{error ? "Retrying…" : "Connecting…"}</Badge>
      )
    case "failed":
      return <Badge variant="destructive">Failed</Badge>
    default:
      return <Badge variant="secondary">Off</Badge>
  }
}

export function MqttCard({
  api,
  liveState,
  connected,
}: {
  api: MqttApi
  /** Faster than polling: the state the device reports over the WebSocket. */
  liveState?: MqttState
  connected: boolean
}) {
  const { config, loadError, save } = useMqtt(api)
  const [draft, setDraft] = useState<Draft | null>(null)
  const [saving, setSaving] = useState(false)
  const [formError, setFormError] = useState<string | null>(null)
  const [savedAt, setSavedAt] = useState(0)

  // `draft` is null until the user edits something: before that the form simply
  // mirrors the device. This avoids copying props into state in an effect.
  const form: Draft | null = draft ?? (config ? toDraft(config) : null)
  const update = (patch: Partial<Draft>) =>
    form && setDraft({ ...form, ...patch })

  const changes = config && form ? diff(config, form) : {}
  const dirty = Object.keys(changes).length > 0
  const state: MqttState = liveState ?? config?.state ?? "disabled"

  const submit = async () => {
    if (!config || !form) return
    const problem = validateMqtt(changes, {
      enabled: !!config.enabled,
      host: config.host,
    })
    if (problem) {
      setFormError(problem)
      return
    }
    setSaving(true)
    setFormError(null)
    try {
      await save(changes)
      setDraft(null)
      setSavedAt(Date.now())
    } catch (e) {
      setFormError(e instanceof Error ? e.message : "Could not save")
    } finally {
      setSaving(false)
    }
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex flex-wrap items-center gap-2">
          Home Assistant
          {config && <StatusBadge state={state} error={config.error} />}
        </CardTitle>
        <CardDescription>
          Publish motion and presence over MQTT. Home Assistant finds the
          device by itself.
        </CardDescription>
      </CardHeader>

      <CardContent className="flex flex-col gap-5">
        {!config || !form ? (
          loadError ? (
            <Alert variant="destructive">
              <TriangleAlertIcon />
              <AlertTitle>Settings unavailable</AlertTitle>
              <AlertDescription>{loadError}</AlertDescription>
            </Alert>
          ) : (
            <div className="flex flex-col gap-3">
              <Skeleton className="h-8 w-full" />
              <Skeleton className="h-8 w-full" />
              <Skeleton className="h-8 w-2/3" />
            </div>
          )
        ) : (
          <>
            {state === "failed" || (state === "connecting" && config.error) ? (
              <Alert variant="destructive">
                <TriangleAlertIcon />
                <AlertTitle>Can't reach the broker</AlertTitle>
                <AlertDescription>
                  {config.error}. Retrying automatically.
                </AlertDescription>
              </Alert>
            ) : null}

            <FieldGroup>
              <Field orientation="horizontal">
                <FieldContent>
                  <FieldLabel htmlFor="mqtt-enabled">Enable MQTT</FieldLabel>
                  <FieldDescription>
                    Needs an MQTT broker, such as the Mosquitto add-on in Home
                    Assistant.
                  </FieldDescription>
                </FieldContent>
                <Switch
                  id="mqtt-enabled"
                  checked={form.enabled}
                  onCheckedChange={(v) => update({ enabled: v })}
                  disabled={!connected}
                />
              </Field>

              <div className="grid gap-4 sm:grid-cols-[1fr_6.5rem]">
                <Field>
                  <FieldLabel htmlFor="mqtt-host">Broker address</FieldLabel>
                  <Input
                    id="mqtt-host"
                    value={form.host}
                    onChange={(e) => update({ host: e.target.value })}
                    placeholder="192.168.1.10"
                    autoComplete="off"
                    autoCapitalize="off"
                    spellCheck={false}
                    inputMode="url"
                    disabled={!connected}
                  />
                </Field>
                <Field>
                  <FieldLabel htmlFor="mqtt-port">Port</FieldLabel>
                  <Input
                    id="mqtt-port"
                    value={form.port}
                    onChange={(e) => update({ port: e.target.value })}
                    inputMode="numeric"
                    disabled={!connected}
                  />
                </Field>
              </div>
              <FieldDescription className="-mt-2">
                Use the broker's IP address. Names ending in .local are not
                supported.
              </FieldDescription>

              <div className="grid gap-4 sm:grid-cols-2">
                <Field>
                  <FieldLabel htmlFor="mqtt-user">Username</FieldLabel>
                  <Input
                    id="mqtt-user"
                    value={form.user}
                    onChange={(e) => update({ user: e.target.value })}
                    placeholder="optional"
                    autoComplete="off"
                    autoCapitalize="off"
                    spellCheck={false}
                    disabled={!connected}
                  />
                </Field>
                <Field>
                  <FieldLabel htmlFor="mqtt-pass">Password</FieldLabel>
                  <Input
                    id="mqtt-pass"
                    type="password"
                    value={form.pass}
                    onChange={(e) =>
                      update({ pass: e.target.value, clearPass: false })
                    }
                    placeholder={
                      config.hasPass && !form.clearPass
                        ? "•••••••• (saved)"
                        : "optional"
                    }
                    autoComplete="new-password"
                    disabled={!connected}
                  />
                  {config.hasPass && !form.clearPass && !form.pass ? (
                    <Button
                      type="button"
                      variant="link"
                      size="xs"
                      className="justify-start px-0"
                      onClick={() => update({ clearPass: true })}
                    >
                      Remove saved password
                    </Button>
                  ) : null}
                </Field>
              </div>

              <Field>
                <FieldLabel htmlFor="mqtt-prefix">Discovery prefix</FieldLabel>
                <Input
                  id="mqtt-prefix"
                  value={form.prefix}
                  onChange={(e) => update({ prefix: e.target.value })}
                  autoComplete="off"
                  autoCapitalize="off"
                  spellCheck={false}
                  disabled={!connected}
                />
                <FieldDescription>
                  Leave as "homeassistant" unless you changed it in Home
                  Assistant.
                </FieldDescription>
              </Field>
            </FieldGroup>

            {formError && (
              <Alert variant="destructive" role="alert">
                <TriangleAlertIcon />
                <AlertDescription>{formError}</AlertDescription>
              </Alert>
            )}

            <div className="flex flex-wrap items-center gap-3">
              <Button
                size="lg"
                onClick={submit}
                disabled={!connected || !dirty || saving}
              >
                {saving && <Spinner data-icon="inline-start" />}
                Save
              </Button>
              {!dirty && savedAt > 0 && !formError && (
                <span className="inline-flex items-center gap-1.5 text-sm text-muted-foreground">
                  <CircleCheckIcon className="size-4 text-ok" />
                  Saved on the device
                </span>
              )}
            </div>

            {state === "connected" && (
              <div className="flex flex-col gap-2 rounded-lg bg-muted/50 p-3 text-sm">
                <p>
                  In Home Assistant, look for the device{" "}
                  <span className="font-medium">
                    Ripple {config.id.replace("ripple_", "")}
                  </span>{" "}
                  (Settings → Devices &amp; services → MQTT). It provides:
                </p>
                <ul className="flex flex-wrap gap-1.5">
                  {ENTITIES.map((e) => (
                    <li key={e}>
                      <Badge variant="outline">{e}</Badge>
                    </li>
                  ))}
                </ul>
                <p className="text-xs text-muted-foreground">
                  Topics start with{" "}
                  <code className="font-mono">{config.base}</code>. Motion and
                  presence are unavailable in Home Assistant until the board
                  is calibrated.
                </p>
              </div>
            )}
          </>
        )}
      </CardContent>
    </Card>
  )
}
