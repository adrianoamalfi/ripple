import { parseState } from "@/lib/protocol"
import type {
  Command,
  ConnectionStatus,
  DeviceState,
  Transport,
} from "@/lib/types"

interface SocketOptions {
  /** No message for this long means the link is dead (the browser does not
   * always notice when the board reboots), so reconnect. */
  staleMs?: number
  retryMs?: number
}

export function createSocketTransport(
  url: string,
  { staleMs = 4000, retryMs = 1500 }: SocketOptions = {}
): Transport {
  let ws: WebSocket | null = null
  let stopped = true
  let status: ConnectionStatus = "closed"
  let lastMessage = 0
  let retryTimer: ReturnType<typeof setTimeout> | undefined
  let watchdog: ReturnType<typeof setInterval> | undefined
  let stateCb: (s: DeviceState) => void = () => {}
  let statusCb: (s: ConnectionStatus) => void = () => {}

  const setStatus = (next: ConnectionStatus) => {
    if (next === status) return
    status = next
    statusCb(next)
  }

  const detach = () => {
    if (!ws) return
    ws.onopen = ws.onclose = ws.onerror = ws.onmessage = null
    try {
      ws.close()
    } catch {
      /* already closed */
    }
    ws = null
  }

  const scheduleRetry = () => {
    if (stopped) return
    clearTimeout(retryTimer)
    retryTimer = setTimeout(connect, retryMs)
  }

  function connect() {
    if (stopped) return
    detach()
    setStatus("connecting")
    const socket = new WebSocket(url)
    ws = socket
    socket.onopen = () => {
      lastMessage = Date.now()
      setStatus("open")
    }
    socket.onmessage = (e) => {
      const state = typeof e.data === "string" ? parseState(e.data) : null
      if (!state) return
      lastMessage = Date.now()
      setStatus("open")
      stateCb(state)
    }
    socket.onclose = () => {
      setStatus("closed")
      scheduleRetry()
    }
    socket.onerror = () => socket.close()
  }

  return {
    start() {
      if (!stopped) return
      stopped = false
      connect()
      watchdog = setInterval(() => {
        if (status === "open" && Date.now() - lastMessage > staleMs) {
          setStatus("stalled")
          connect()
        }
      }, 1000)
    },
    stop() {
      stopped = true
      clearTimeout(retryTimer)
      clearInterval(watchdog)
      detach()
      setStatus("closed")
    },
    send(command: Command) {
      if (!ws || ws.readyState !== WebSocket.OPEN || status !== "open") return false
      ws.send(JSON.stringify(command))
      return true
    },
    onState(cb) {
      stateCb = cb
    },
    onStatus(cb) {
      statusCb = cb
    },
  }
}
