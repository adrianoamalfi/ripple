import { useCallback, useEffect, useRef, useState } from "react"

import { pushSample, type Bucket } from "@/lib/history"
import type {
  Command,
  ConnectionStatus,
  DeviceState,
  Transport,
} from "@/lib/types"

type Ack = (state: DeviceState) => boolean

interface Waiter {
  check: Ack
  done: (ok: boolean) => void
}

const RETRY_MS = 1500
const GIVE_UP_MS = 4500

export class CommandError extends Error {}

/** Connects to the device and exposes its state, a per-second history and a
 * `run` that sends a command and waits for the device to confirm it. The
 * firmware may drop a command (e.g. right after a reboot), so an unconfirmed
 * command is sent once more before giving up. All commands are idempotent. */
export function useRipple(createTransport: () => Transport) {
  const [state, setState] = useState<DeviceState | null>(null)
  const [status, setStatus] = useState<ConnectionStatus>("connecting")
  // Wall clock, refreshed once a second: drives relative times and the chart.
  const [nowMs, setNowMs] = useState(() => Date.now())

  const transport = useRef<Transport | null>(null)
  const latest = useRef<DeviceState | null>(null)
  const history = useRef<Bucket[]>([])
  const waiters = useRef(new Set<Waiter>())

  useEffect(() => {
    const t = createTransport()
    transport.current = t
    t.onStatus(setStatus)
    t.onState((s) => {
      latest.current = s
      history.current = pushSample(history.current, s, Date.now())
      setState(s)
      for (const w of [...waiters.current]) if (w.check(s)) w.done(true)
    })
    t.start()
    return () => {
      t.stop()
      transport.current = null
    }
    // The transport is created once for the lifetime of the page.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  useEffect(() => {
    const id = setInterval(() => setNowMs(Date.now()), 1000)
    return () => clearInterval(id)
  }, [])

  const run = useCallback((command: Command, check: Ack) => {
    return new Promise<void>((resolve, reject) => {
      const t = transport.current
      const timers: {
        retry?: ReturnType<typeof setTimeout>
        giveUp?: ReturnType<typeof setTimeout>
      } = {}
      let finished = false

      const waiter: Waiter = {
        check,
        done: (ok) => {
          if (finished) return
          finished = true
          clearTimeout(timers.retry)
          clearTimeout(timers.giveUp)
          waiters.current.delete(waiter)
          if (ok) resolve()
          else reject(new CommandError("The device did not respond. Try again."))
        },
      }

      if (!t || !t.send(command)) {
        reject(new CommandError("Not connected to the device."))
        return
      }
      waiters.current.add(waiter)
      timers.retry = setTimeout(() => transport.current?.send(command), RETRY_MS)
      timers.giveUp = setTimeout(() => waiter.done(false), GIVE_UP_MS)
    })
  }, [])

  const getHistory = useCallback(() => history.current, [])

  return { state, status, run, getHistory, nowMs }
}
