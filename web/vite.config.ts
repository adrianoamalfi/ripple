import { resolve } from "node:path"
import tailwindcss from "@tailwindcss/vite"
import react from "@vitejs/plugin-react"
import { defineConfig } from "vitest/config"
import { viteSingleFile } from "vite-plugin-singlefile"

// The dashboard is served by an ESP8266 with ~1 MB of flash to spare, so the
// build is one self-contained index.html (JS and CSS inlined, no fonts, no
// CDN). `npm run build` then gzips it into ../firmware_assets/.
export default defineConfig({
  plugins: [react(), tailwindcss(), viteSingleFile()],
  resolve: {
    alias: {
      "@": resolve(import.meta.dirname, "./src"),
    },
  },
  build: {
    target: "es2020",
    chunkSizeWarningLimit: 2000,
  },
  server: {
    host: true,
    // `VITE_DEVICE=192.168.1.50 npm run dev` talks to a real board; its REST API
    // is proxied so the browser does not run into CORS.
    proxy: process.env.VITE_DEVICE
      ? { "/api": { target: `http://${process.env.VITE_DEVICE}`, changeOrigin: true } }
      : undefined,
  },
  test: {
    environment: "node",
  },
})
