import { validateMqtt } from "@/lib/mqtt-validate"
import type { MqttConfig, MqttUpdate } from "@/lib/types"

export interface MqttApi {
  get(): Promise<MqttConfig>
  save(update: MqttUpdate): Promise<MqttConfig>
}

async function readJson(res: Response): Promise<{ error?: string } | null> {
  try {
    return await res.json()
  } catch {
    return null
  }
}

/** The real device: GET / POST /api/mqtt. */
export function createHttpMqttApi(
  fetchFn: typeof fetch = (...args) => fetch(...args)
): MqttApi {
  return {
    async get() {
      const res = await fetchFn("/api/mqtt", { cache: "no-store" })
      if (!res.ok) throw new Error(`The device answered ${res.status}`)
      return (await res.json()) as MqttConfig
    },
    async save(update) {
      const res = await fetchFn("/api/mqtt", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(update),
      })
      const body = await readJson(res)
      if (!res.ok) throw new Error(body?.error ?? `The device answered ${res.status}`)
      return body as unknown as MqttConfig
    },
  }
}

/** A simulated broker connection for the demo mode. A host starting with
 * "bad" is unreachable and the user "wrong" gets rejected. */
export function createDemoMqttApi(now: () => number = Date.now): MqttApi {
  let cfg: MqttConfig = {
    enabled: 0, host: "", port: 1883, user: "", hasPass: 0,
    prefix: "homeassistant", state: "disabled", error: "",
    id: "ripple_demo01", base: "ripple/ripple_demo01",
  }
  let changedAt = 0

  const view = (): MqttConfig => {
    if (!cfg.enabled || !cfg.host) return { ...cfg, state: "disabled", error: "" }
    if (now() - changedAt < 1500) return { ...cfg, state: "connecting", error: "" }
    if (cfg.host.startsWith("bad")) return { ...cfg, state: "failed", error: "Broker unreachable" }
    if (cfg.user === "wrong") return { ...cfg, state: "failed", error: "Wrong user or password" }
    return { ...cfg, state: "connected", error: "" }
  }

  return {
    async get() {
      return view()
    },
    async save(update) {
      const problem = validateMqtt(update, { enabled: !!cfg.enabled, host: cfg.host })
      if (problem) throw new Error(problem)
      cfg = {
        ...cfg,
        enabled: update.enabled === undefined ? cfg.enabled : update.enabled ? 1 : 0,
        host: update.host ?? cfg.host,
        port: update.port ?? cfg.port,
        user: update.user ?? cfg.user,
        hasPass: update.pass === undefined ? cfg.hasPass : update.pass ? 1 : 0,
        prefix: update.prefix ?? cfg.prefix,
      }
      changedAt = now()
      return view()
    },
  }
}
