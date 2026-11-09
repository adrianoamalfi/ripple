// Fails when firmware_assets/dashboard.html.gz does not match a fresh build of
// web/. Compares the decompressed HTML (not the gzip bytes, which can differ
// between zlib versions).
import { gunzipSync } from "node:zlib"
import { readFileSync } from "node:fs"
import { dirname, resolve } from "node:path"
import { fileURLToPath } from "node:url"

const here = dirname(fileURLToPath(import.meta.url))
const fresh = readFileSync(resolve(here, "../dist/index.html"))
const committed = gunzipSync(readFileSync(resolve(here, "../../firmware_assets/dashboard.html.gz")))

if (Buffer.compare(fresh, committed) !== 0) {
  console.error(
    "firmware_assets/dashboard.html.gz is out of date with web/.\n" +
      "Run `npm run build` in web/ and commit the updated file."
  )
  process.exit(1)
}
console.log("firmware_assets/dashboard.html.gz matches a fresh build.")
