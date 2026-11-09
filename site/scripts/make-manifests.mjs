// Writes one ESP Web Tools manifest per board for the web installer.
//   node make-manifests.mjs <firmware dir> <output dir> <tag>
// The firmware dir holds ripple-<board>-<tag>.bin files (the release assets); the
// manifests are written next to the installer page and reference them as
// ../firmware/<file>, so they are served from the same origin (no CORS issues).
import { readdirSync, writeFileSync } from "node:fs"
import { join } from "node:path"

const [firmwareDir, outDir, tag] = process.argv.slice(2)
if (!firmwareDir || !outDir || !tag) {
  console.error("usage: make-manifests.mjs <firmware dir> <output dir> <tag>")
  process.exit(2)
}
const version = tag.replace(/^v/, "")
let written = 0
for (const file of readdirSync(firmwareDir)) {
  const m = file.match(/^ripple-([a-z0-9_]+)-v?[\w.+-]+\.bin$/)
  if (!m) continue
  const manifest = {
    name: "Ripple",
    version,
    new_install_prompt_erase: true,
    builds: [{ chipFamily: "ESP8266", parts: [{ path: `../firmware/${file}`, offset: 0 }] }],
  }
  writeFileSync(join(outDir, `manifest-${m[1]}.json`), JSON.stringify(manifest, null, 2) + "\n")
  console.log(`manifest-${m[1]}.json -> ${file}`)
  written++
}
if (!written) {
  console.error("no firmware files found")
  process.exit(1)
}
