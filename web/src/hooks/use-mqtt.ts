import { useCallback, useEffect, useState } from "react"

import type { MqttApi } from "@/lib/mqtt-api"
import type { MqttConfig, MqttUpdate } from "@/lib/types"

const POLL_MS = 4000

/** MQTT settings and connection status. Polled, because the broker link can
 * change without anyone touching the page (broker restarts, wrong password). */
export function useMqtt(api: MqttApi) {
  const [config, setConfig] = useState<MqttConfig | null>(null)
  const [loadError, setLoadError] = useState<string | null>(null)

  useEffect(() => {
    let alive = true
    const load = async () => {
      try {
        const c = await api.get()
        if (!alive) return
        setConfig(c)
        setLoadError(null)
      } catch (e) {
        if (alive) setLoadError(e instanceof Error ? e.message : "Could not load")
      }
    }
    void load()
    const id = setInterval(load, POLL_MS)
    return () => {
      alive = false
      clearInterval(id)
    }
  }, [api])

  const save = useCallback(
    async (update: MqttUpdate) => {
      const c = await api.save(update)
      setConfig(c)
      return c
    },
    [api]
  )

  return { config, loadError, save }
}
