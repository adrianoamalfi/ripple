export interface WifiApi {
  /** Deletes the saved network and restarts the board into its setup portal. */
  forget(): Promise<void>
}

export function createHttpWifiApi(
  fetchFn: typeof fetch = (...args) => fetch(...args)
): WifiApi {
  return {
    async forget() {
      const res = await fetchFn("/api/wifi/reset", { method: "POST" })
      if (!res.ok) throw new Error(`The device answered ${res.status}`)
    },
  }
}

/** Demo mode: nothing to restart. */
export function createDemoWifiApi(): WifiApi {
  return { async forget() {} }
}

/** "ripple-4f7ddc" -> "Ripple-4f7ddc": the name of the setup network. */
export function setupNetworkName(host: string | undefined): string {
  return host ? host.replace(/^ripple-/, "Ripple-") : "Ripple-…"
}
