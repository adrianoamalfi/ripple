# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/). Versions below 1.0 may change behavior
between releases.

## [0.1.0] - 2026-11-09

First public pre-release.

### Added

- Motion and presence detection from the RSSI of the link to the router: a 2-second
  sliding-window variance against a calibrated threshold, with hysteresis and a
  30-second presence hold.
- A guided calibration (empty room, walking, sitting still, empty room again) that
  derives the threshold from the room's own data and reports a quality verdict.
  Quick calibration and a manual threshold are available too. Calibration is saved in
  flash.
- A web dashboard served by the board: live signal chart, status, calibration,
  sensitivity, MQTT settings and device info. Light and dark themes, works on a
  phone. It has a built-in demo mode with a simulated board.
- MQTT with Home Assistant discovery: motion, presence, signal strength, signal
  fluctuation, a motion threshold you can change and a calibration button, with
  availability and Last Will.
- WiFi setup through a captive portal (`Ripple-xxxxxx`), a `ripple-xxxxxx.local` name,
  and a way to forget the saved network.
- REST and WebSocket interfaces, serial commands, and a project website with a live
  demo and a web installer.
- Unit tests for the algorithm, the calibration, the Home Assistant messages and the
  WiFi helpers, and continuous integration.

### Known limitations

- No dashboard password and no signed over-the-air updates yet. Update over USB.
- The presence timeout is fixed at 30 seconds.
- A person sitting perfectly still cannot be detected with the RSSI alone.

[0.1.0]: https://github.com/adrianoamalfi/ripple/releases/tag/v0.1.0
