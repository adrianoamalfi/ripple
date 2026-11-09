import { describe, expect, it, vi } from "vitest"

import { createHttpWifiApi, setupNetworkName } from "@/lib/wifi-api"

describe("wifi api", () => {
  it("posts to /api/wifi/reset", async () => {
    const f = vi.fn().mockResolvedValue(new Response("{}", { status: 200 }))
    await createHttpWifiApi(f as unknown as typeof fetch).forget()
    expect(f).toHaveBeenCalledWith("/api/wifi/reset", { method: "POST" })
  })

  it("reports a failing device", async () => {
    const f = vi.fn().mockResolvedValue(new Response("nope", { status: 500 }))
    await expect(createHttpWifiApi(f as unknown as typeof fetch).forget()).rejects.toThrow("500")
  })

  it("names the setup network from the host name", () => {
    expect(setupNetworkName("ripple-4f7ddc")).toBe("Ripple-4f7ddc")
    expect(setupNetworkName(undefined)).toBe("Ripple-…")
  })
})
