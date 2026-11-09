import { describe, expect, it, vi } from "vitest"

import { createDemoMqttApi, createHttpMqttApi } from "@/lib/mqtt-api"
import { validateMqtt } from "@/lib/mqtt-validate"

const off = { enabled: false, host: "" }

describe("validateMqtt", () => {
  it("accepts ordinary settings", () => {
    expect(validateMqtt({ enabled: true, host: "192.168.1.10", port: 1883 }, off)).toBeNull()
    expect(validateMqtt({ host: "broker.home-lan_1" }, off)).toBeNull()
    expect(validateMqtt({ prefix: "homeassistant" }, off)).toBeNull()
    expect(validateMqtt({ prefix: "ha/custom" }, off)).toBeNull()
  })

  it("rejects what the firmware rejects", () => {
    expect(validateMqtt({ host: "bad host!" }, off)).toMatch(/Host may only/)
    expect(validateMqtt({ host: "x".repeat(64) }, off)).toMatch(/Host may only/)
    expect(validateMqtt({ port: 0 }, off)).toMatch(/Port/)
    expect(validateMqtt({ port: 70000 }, off)).toMatch(/Port/)
    expect(validateMqtt({ port: 18.5 }, off)).toMatch(/Port/)
    expect(validateMqtt({ user: 'a"b' }, off)).toMatch(/user name/)
    expect(validateMqtt({ user: "back\\slash" }, off)).toMatch(/user name/)
    expect(validateMqtt({ user: "x".repeat(32) }, off)).toMatch(/user name/)
    expect(validateMqtt({ pass: "x".repeat(64) }, off)).toMatch(/too long/)
    expect(validateMqtt({ pass: "tab\there" }, off)).toMatch(/Invalid password/)
    expect(validateMqtt({ prefix: "/leading" }, off)).toMatch(/prefix/)
    expect(validateMqtt({ prefix: "trailing/" }, off)).toMatch(/prefix/)
    expect(validateMqtt({ prefix: "" }, off)).toMatch(/prefix/)
  })

  it("accepts a password with symbols and quotes (it is never echoed back)", () => {
    expect(validateMqtt({ pass: 'p@ss"w\\rd é!' }, off)).toBeNull()
  })

  it("needs a host to enable, whether it is new or already stored", () => {
    expect(validateMqtt({ enabled: true }, off)).toBe("Host is required")
    expect(validateMqtt({ enabled: true }, { enabled: false, host: "10.0.0.2" })).toBeNull()
    expect(validateMqtt({ host: "" }, { enabled: true, host: "10.0.0.2" })).toBe("Host is required")
  })
})

describe("createHttpMqttApi", () => {
  const json = (body: unknown, status = 200) =>
    Promise.resolve(new Response(JSON.stringify(body), { status }))

  it("returns the device's error message", async () => {
    const f = vi.fn().mockReturnValue(json({ error: "Port must be between 1 and 65535" }, 400))
    const api = createHttpMqttApi(f as unknown as typeof fetch)
    await expect(api.save({ port: 0 })).rejects.toThrow("Port must be between 1 and 65535")
  })

  it("falls back to the status code when the body is not JSON", async () => {
    const f = vi.fn().mockResolvedValue(new Response("oops", { status: 500 }))
    const api = createHttpMqttApi(f as unknown as typeof fetch)
    await expect(api.save({})).rejects.toThrow("500")
    await expect(api.get()).rejects.toThrow("500")
  })

  it("posts JSON to /api/mqtt", async () => {
    const cfg = { enabled: 1, host: "h", port: 1883, user: "", hasPass: 0, prefix: "homeassistant", state: "connecting", error: "", id: "i", base: "b" }
    const f = vi.fn().mockReturnValue(json(cfg))
    const api = createHttpMqttApi(f as unknown as typeof fetch)
    const out = await api.save({ enabled: true, host: "h" })
    expect(out.state).toBe("connecting")
    const [url, init] = f.mock.calls[0]
    expect(url).toBe("/api/mqtt")
    expect(init.method).toBe("POST")
    expect(JSON.parse(init.body)).toEqual({ enabled: true, host: "h" })
  })
})

describe("createDemoMqttApi", () => {
  it("connects after a moment and never exposes the password", async () => {
    let t = 1000
    const api = createDemoMqttApi(() => t)
    expect((await api.get()).state).toBe("disabled")

    const saved = await api.save({ enabled: true, host: "192.168.1.10", user: "u", pass: "secret" })
    expect(saved.state).toBe("connecting")
    expect(JSON.stringify(saved)).not.toContain("secret")
    expect(saved.hasPass).toBe(1)

    t += 2000
    expect((await api.get()).state).toBe("connected")
  })

  it("keeps the stored password when none is sent, and clears it on request", async () => {
    const api = createDemoMqttApi(() => 0)
    await api.save({ host: "10.0.0.2", pass: "secret" })
    expect((await api.save({ port: 1884 })).hasPass).toBe(1)
    expect((await api.save({ pass: "" })).hasPass).toBe(0)
  })

  it("reports unreachable brokers and bad credentials", async () => {
    let t = 0
    const api = createDemoMqttApi(() => t)
    await api.save({ enabled: true, host: "bad.example" })
    t += 2000
    expect(await api.get()).toMatchObject({ state: "failed", error: "Broker unreachable" })

    await api.save({ host: "10.0.0.2", user: "wrong" })
    t += 2000
    expect(await api.get()).toMatchObject({ state: "failed", error: "Wrong user or password" })
  })

  it("validates like the device does", async () => {
    const api = createDemoMqttApi()
    await expect(api.save({ enabled: true })).rejects.toThrow("Host is required")
    await expect(api.save({ port: 99999 })).rejects.toThrow("Port")
  })
})
