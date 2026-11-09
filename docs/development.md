# Development

## Prerequisites

- [PlatformIO](https://platformio.org/install) for the firmware.
- Node.js 24 and npm, only if you change the dashboard or the site.

## Firmware

```bash
pio run -e nodemcuv2                 # build (also d1_mini, esp12e)
pio run -e nodemcuv2 -t upload       # build and flash over USB
python tools/monitor.py              # serial monitor (see below)
pio test -e native                   # unit tests, no hardware needed
```

The version comes from the [`VERSION`](../VERSION) file.

**Developer shortcut:** copy `include/secrets.h.example` to `include/secrets.h` and
fill in your WiFi. When no network is saved on the board, that network is used
instead of opening the setup portal. It is git-ignored; delete it to test the
real first-boot experience. Serial `W` joins it through the portal code path
without anyone typing a password.

`tools/monitor.py` is a small serial monitor that drops the stale bytes some
USB-serial drivers replay when the port opens, and logs to `.pio/monitor.log`.

## Dashboard

```bash
cd web
npm ci
npm run dev          # opens against a simulated board, no hardware needed
npm test             # unit tests (Vitest)
npm run lint
npm run build        # builds, gzips into ../firmware_assets/ and checks the size budget
```

- `VITE_DEVICE=192.168.1.50 npm run dev` runs the dev server against a real board.
- `?demo` on any build uses the simulator; `?demo=poor` simulates a bad calibration
  result and `?demo=fast` shortens the wizard.
- After `npm run build`, rebuild the firmware to embed the new dashboard. The
  committed `firmware_assets/dashboard.html.gz` is what the firmware builds embed,
  so contributors who don't touch the dashboard never need Node.js. CI fails if the
  committed file is out of date with `web/`.
- Size budget: **300 KB gzipped** (currently about 235 KB), enforced by the build.

## Project site

The site in [`site/`](../site) is static HTML and CSS, published to GitHub Pages.
The live demo is the dashboard built with `npm run build:demo`, and the web
installer is fed by the firmware attached to the latest release. Both are assembled
by the Pages workflow; to preview the page itself, open `site/index.html`.

The images in `site/assets/img` and `docs/images` are generated from the demo mode:

```bash
cd web && npm run build:demo
cd ../tools/screenshots && npm install && npx playwright install chromium
npm run capture
```

## Tests

| Where | What |
|---|---|
| `test/test_core` | The detector: variance, calibration, motion, presence |
| `test/test_calibrator` | The guided calibration state machine |
| `test/test_ha` | Home Assistant topics and discovery payloads |
| `test/test_wifi` | WiFi input validation, names, JSON escaping |
| `web/src/lib/__tests__` | Dashboard logic and the simulated board |

Hardware behaviour (WiFi, MQTT, the portal) is checked by hand on a board; see the
pull request template for the checklist.

## Continuous integration and releases

- **CI** (`.github/workflows/ci.yml`) runs on every push and pull request: the
  firmware tests, a build for all three boards, and the dashboard's type check,
  lint, tests and build, which must leave the committed dashboard asset unchanged.
- **Pages** (`pages.yml`) publishes the site, the live demo and the web installer
  whenever `site/` or `web/` change on `main`, and after each release.
- **Release** (`release.yml`) runs when you push a tag like `v0.2.0`. It checks the
  tag against `VERSION`, builds the three boards, writes `SHA256SUMS`, and publishes
  a GitHub release with the notes from the changelog. Versions below 1.0 are marked
  as pre-releases.

To make a release: update `VERSION` and `CHANGELOG.md`, merge to `main`, then

```bash
git tag v0.2.0 && git push origin v0.2.0
```

## Pull requests

Keep them focused, add tests for logic, and say how you tested on hardware if the
change touches the firmware. See [CONTRIBUTING](../CONTRIBUTING.md).
