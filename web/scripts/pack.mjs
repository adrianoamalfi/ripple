// Gzips the single-file build into ../firmware_assets/dashboard.html.gz, the
// asset the firmware embeds (see tools/embed_dashboard.py).
import { gzipSync } from "node:zlib"
import { readFileSync, writeFileSync, mkdirSync } from "node:fs"
import { resolve, dirname } from "node:path"
import { fileURLToPath } from "node:url"

const here = dirname(fileURLToPath(import.meta.url))
const src = resolve(here, "../dist/index.html")
const out = resolve(here, "../../firmware_assets/dashboard.html.gz")

// Budget: the firmware must stay well below the ~1 MB OTA limit of a 4 MB
// ESP8266, and the page should load quickly over the board's WiFi.
const BUDGET_KB = 300

const html = readFileSync(src)
const gz = gzipSync(html, { level: 9 })
mkdirSync(dirname(out), { recursive: true })
writeFileSync(out, gz)

const kb = (n) => (n / 1024).toFixed(1)
console.log(`dashboard: ${kb(html.length)} KB raw -> ${kb(gz.length)} KB gzip (budget ${BUDGET_KB} KB)`)
console.log(`written ${out}`)
if (gz.length > BUDGET_KB * 1024) {
  console.error(`over budget by ${kb(gz.length - BUDGET_KB * 1024)} KB`)
  process.exit(1)
}
