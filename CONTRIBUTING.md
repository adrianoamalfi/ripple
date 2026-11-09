# Contributing to Ripple

Thank you for helping! Ripple is a small project and every kind of contribution is
welcome: bug reports, real-world results, documentation, code.

## The most useful thing you can do

**Tell us how it works in your home.** Open an issue with your board, the room, where
you put the board and the result of the guided calibration (quality, noise, how much
walking it saw). Positive and negative reports both help: they decide what to improve.

## Reporting a bug

Use the *Bug report* issue template. The most useful details are the firmware version
(shown in the dashboard), the board model, what you expected, and the serial log at
115200 baud if the board restarts or misbehaves. **Never paste your WiFi or MQTT
passwords.**

## Suggesting a feature

Open an issue first for anything sizeable, so we can agree on the shape before you
spend time on it.

## Sending code

1. Fork the repository and create a branch.
2. Set up as described in [docs/development.md](docs/development.md).
3. Make a focused change. Match the surrounding code's style and comment density.
4. Add or update tests: `pio test -e native` for the algorithm and the pure logic,
   `npm test` in `web/` for the dashboard.
5. Run everything CI runs (below) and, for firmware changes, try it on a board.
6. Open a pull request and fill in the template.

Before you push:

```bash
pio test -e native
pio run -e nodemcuv2 -e d1_mini -e esp12e
cd web && npm ci && npm run lint && npm test && npm run build
```

If you change anything under `web/`, commit the rebuilt
`firmware_assets/dashboard.html.gz` too. CI checks it.

## Guidelines

- The firmware is memory constrained (roughly 25–30 KB of heap free). Avoid heap churn in
  the sampling path and keep new buffers small.
- Anything that decides what the sensor *concludes* belongs in `lib/ripple_core`,
  free of Arduino headers, with tests.
- No telemetry, no cloud dependency, no external requests from the firmware or the
  dashboard.
- Be honest about limits in the docs: a clear "this doesn't work well when..." is
  worth more than a promise.

## Code of conduct

Be kind and assume good faith. This project follows the
[Contributor Covenant](CODE_OF_CONDUCT.md).

## License

By contributing you agree that your contributions are licensed under the
[MIT License](LICENSE).
