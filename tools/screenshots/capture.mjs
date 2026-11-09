// Regenerates the screenshots used by the README and the project site.
//
//   cd web && npm run build:demo        # builds web/dist-demo/index.html
//   cd tools/screenshots && npm install && npx playwright install chromium
//   npm run capture
//
// The dashboard is loaded in its demo mode (a simulated board), so no hardware
// is needed. The WiFi setup page is taken from src/portal_page.h with the board's
// API mocked.
import { chromium } from "playwright"
import { copyFileSync, mkdirSync, readFileSync } from "node:fs"
import { dirname, resolve } from "node:path"
import { fileURLToPath, pathToFileURL } from "node:url"

const here = dirname(fileURLToPath(import.meta.url))
const root = resolve(here, "../..")
const demoFile = resolve(root, "web/dist-demo/index.html")
const outDirs = [resolve(root, "site/assets/img"), resolve(root, "docs/images")]
outDirs.forEach((d) => mkdirSync(d, { recursive: true }))

const demoUrl = (flags = "fast") => `${pathToFileURL(demoFile).href}?demo=${flags}`
const jpeg = { type: "jpeg", quality: 90 }

async function save(shot, name) {
  const first = resolve(outDirs[0], name)
  await shot(first)
  for (const d of outDirs.slice(1)) copyFileSync(first, resolve(d, name))
  console.log("wrote", name)
}

const browser = await chromium.launch()
// The demo's "simulated board" notice is right for the demo, wrong for marketing shots.
const hideDemoBanner = (page) => page.addStyleTag({ content: "[data-demo-banner]{display:none!important}" })

// Desktop and phone, light and dark. Each page runs for a while first so the chart
// has history (a quiet stretch, a motion episode, the threshold) and then the same
// page is shot in both colour schemes.
async function warmDashboard(viewport, scale) {
  const ctx = await browser.newContext({ viewport, deviceScaleFactor: scale, colorScheme: "light" })
  const page = await ctx.newPage()
  await page.goto(demoUrl())
  await hideDemoBanner(page)
  await page.waitForTimeout(40000)
  // End on a motion episode so the shot shows one.
  await page.getByText("Motion detected").first().waitFor({ timeout: 60000 })
  await page.waitForTimeout(2500)
  return { ctx, page }
}

const [desktop, phone] = await Promise.all([
  warmDashboard({ width: 1240, height: 1180 }, 2),
  warmDashboard({ width: 390, height: 844 }, 3),
])
for (const scheme of ["light", "dark"]) {
  await desktop.page.emulateMedia({ colorScheme: scheme })
  await phone.page.emulateMedia({ colorScheme: scheme })
  await desktop.page.waitForTimeout(400)
  await save((p) => desktop.page.screenshot({ path: p, ...jpeg }), `dashboard-${scheme}.jpg`)
  await save((p) => phone.page.screenshot({ path: p, ...jpeg }), `mobile-${scheme}.jpg`)
}
await desktop.ctx.close()
await phone.ctx.close()

// Calibration wizard result, and the Home Assistant card.
{
  const ctx = await browser.newContext({ viewport: { width: 1100, height: 1400 }, deviceScaleFactor: 2, colorScheme: "light" })
  const page = await ctx.newPage()
  await page.goto(demoUrl("fast"))
  await hideDemoBanner(page)
  await page.getByRole("button", { name: "Start guided calibration" }).click()
  for (let step = 0; step < 4; step++) {
    await page.getByRole("button", { name: /I'm in position/ }).click()
    // The card shows the next step (or the result) once the recording ends.
    await page.waitForFunction(
      (n) => document.body.innerText.includes(n === 3 ? "Walking detected" : `Step ${n + 2}`) || !!document.querySelector("[aria-current=step]"),
      step,
      { timeout: 30000 }
    )
    await page.waitForTimeout(500)
  }
  await page.getByText("Walking detected").waitFor({ timeout: 30000 })
  const calCard = page.locator('[data-slot="card"]').filter({ has: page.getByText("Teach the board") }).first()
  await save((p) => calCard.screenshot({ path: p, ...jpeg }), "calibration-result.jpg")

  await page.getByLabel("Enable MQTT").click()
  await page.getByLabel("Broker address").fill("192.168.1.10")
  await page.getByRole("button", { name: "Save", exact: true }).click()
  await page.getByText("Connected", { exact: true }).first().waitFor({ timeout: 15000 })
  const haCard = page.locator('[data-slot="card"]').filter({ has: page.getByText("Publish motion and presence") }).first()
  await save((p) => haCard.screenshot({ path: p, ...jpeg }), "home-assistant.jpg")
  await ctx.close()
}

// The WiFi setup page served by the board's own network, with the API mocked.
{
  const html = readFileSync(resolve(root, "src/portal_page.h"), "utf8").split('R"RIPPLE_PORTAL(')[1].split(')RIPPLE_PORTAL"')[0]
  const networks = [
    { ssid: "Casa Rossi", rssi: -42, secure: 1 },
    { ssid: "Casa Rossi 5G", rssi: -51, secure: 1 },
    { ssid: "Vicini-2.4", rssi: -68, secure: 1 },
    { ssid: "Guest", rssi: -73, secure: 0 },
    { ssid: "Studio-A12", rssi: -81, secure: 1 },
  ]
  for (const scheme of ["light", "dark"]) {
    const ctx = await browser.newContext({ viewport: { width: 390, height: 780 }, deviceScaleFactor: 3, colorScheme: scheme })
    const page = await ctx.newPage()
    await page.route("http://ripple.setup/**", (route) => {
      const url = route.request().url()
      if (url.includes("/api/wifi/scan")) return route.fulfill({ json: { scanning: 0, networks } })
      return route.fulfill({ contentType: "text/html", body: html })
    })
    await page.goto("http://ripple.setup/")
    await page.locator("li button").first().click()
    await page.waitForTimeout(300)
    await save((p) => page.screenshot({ path: p, ...jpeg }), `portal-${scheme}.jpg`)
    await ctx.close()
  }
}

// Social preview card (1200x630) for the project site.
{
  const shot = pathToFileURL(resolve(root, "site/assets/img/dashboard-dark.jpg")).href
  const html = `<!doctype html><meta charset="utf-8"><style>
    *{box-sizing:border-box;margin:0}
    body{width:1200px;height:630px;overflow:hidden;font:600 20px ui-sans-serif,system-ui,-apple-system,sans-serif;color:#eaf1f6;
      background:radial-gradient(900px 500px at 85% -10%,#12394a,transparent 70%),#0e151b;position:relative}
    .rings{position:absolute;right:-220px;top:-250px;width:900px;height:900px;border-radius:50%;
      background:repeating-radial-gradient(circle,transparent 0 56px,#4cc9de22 56px 58px);mask-image:radial-gradient(circle,#000 20%,transparent 70%)}
    .t{position:absolute;left:72px;top:96px;width:560px}
    .brand{display:flex;align-items:center;gap:14px;font-size:30px;letter-spacing:-.01em;margin-bottom:44px}
    .brand svg{width:46px;height:46px}
    h1{font-size:72px;line-height:.98;letter-spacing:-.045em;font-weight:700}
    h1 em{font-style:normal;color:#5fd0e3}
    p{margin-top:28px;font:400 26px/1.4 ui-sans-serif,system-ui,sans-serif;color:#9fb1bf;max-width:520px}
    img{position:absolute;right:-60px;top:150px;width:640px;border-radius:18px;border:1px solid #ffffff22;box-shadow:0 30px 80px #000a}
    .url{position:absolute;left:72px;bottom:56px;font:500 22px ui-monospace,monospace;color:#5fd0e3}
  </style><div class="rings"></div><div class="t">
    <div class="brand"><svg viewBox="0 0 32 32" fill="none" stroke="#5fd0e3" stroke-width="1.8"><circle cx="16" cy="16" r="3.2" fill="#5fd0e3" stroke="none"/><circle cx="16" cy="16" r="7.6" opacity=".8"/><circle cx="16" cy="16" r="12.6" opacity=".42"/></svg>Ripple</div>
    <h1>Sense motion with nothing but <em>WiFi.</em></h1>
    <p>A €3 ESP8266 as a motion and presence sensor. Open source, local, Home Assistant ready.</p></div>
  <img src="${shot}"><div class="url">adrianoamalfi.github.io/ripple</div>`
  const ctx = await browser.newContext({ viewport: { width: 1200, height: 630 }, deviceScaleFactor: 1 })
  const page = await ctx.newPage()
  await page.setContent(html)
  await page.waitForTimeout(300)
  const out = resolve(root, "site/assets/og-image.png")
  await page.screenshot({ path: out, type: "png" })
  console.log("wrote og-image.png")
  await ctx.close()
}

await browser.close()
