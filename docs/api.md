# API reference

Everything the dashboard does goes through these interfaces, so you can script the
board too. There is **no authentication yet**: keep the board on a trusted network.

## HTTP

| Method and path | What it does |
|---|---|
| `GET /` | The dashboard (or the WiFi setup page while the setup network is open) |
| `GET /lite` | A tiny dependency-free status page, kept as a fallback |
| `GET /api/state` | The current state as JSON (same shape as the WebSocket messages) |
| `GET /api/mqtt` | MQTT settings and connection status. The password is never returned. |
| `POST /api/mqtt` | Change MQTT settings: a JSON object with any of `enabled`, `host`, `port`, `user`, `pass`, `prefix`. A missing `pass` keeps the stored one. |
| `GET /api/wifi/status` | WiFi provisioning status |
| `GET /api/wifi/scan` | Networks in range (only while the setup network is open) |
| `POST /api/wifi` | Try a network: `{"ssid": "...", "pass": "..."}` (only while the setup network is open) |
| `POST /api/wifi/reset` | Forget the saved network and restart into setup mode |

Errors are `{"error": "message"}` with a 4xx status.

## WebSocket

Connect to `ws://<board>/ws`. The board pushes the state about **5 times a second**
as JSON text, and accepts commands as JSON text:

```json
{"cmd": "wizard_start"}
{"cmd": "wizard_ready"}
{"cmd": "wizard_cancel"}
{"cmd": "quick_calibrate"}
{"cmd": "reset_calibration"}
{"cmd": "set_threshold", "value": 3.5}
```

Commands are idempotent and unknown or malformed ones are ignored. Watch the next
state message to see the effect: the board does not send a separate reply.

### State message

```json
{
  "v": 1, "fw": "0.1.0", "ms": 123456, "rssi": -58, "ok": 1,
  "var": 1.23, "peak": 2.10, "thr": 3.10, "mean": 0.90, "sigma": 0.35, "max": 2.40,
  "score": 0.40, "phase": "running", "cal": 1.0,
  "motion": 0, "presence": 0,
  "source": "wizard", "saved": 1, "heap": 31000, "mqtt": "connected",
  "wiz": {"state": "idle", "stage": 1, "name": "...", "left": 0, "total": 30},
  "res": {"quality": "good", "mean": 0.9, "sigma": 0.35, "ceil": 2.4, "thr": 2.76,
          "mov": 0.87, "still": 0, "stillOk": 0, "hold": 0},
  "net": "My WiFi", "host": "ripple-4f7ddc"
}
```

| Field | Meaning |
|---|---|
| `ms` | Uptime in milliseconds |
| `rssi`, `ok` | Signal strength in dBm; `ok` is 0 when the link is down |
| `var` | Variance of the current 2-second window (dB²) |
| `peak` | Highest `var` since the previous message |
| `thr` | Motion threshold; 0 until calibrated |
| `mean`, `sigma`, `max` | What the empty room's noise looked like |
| `score` | How far `var` is between the noise level (0) and the threshold (1) |
| `phase` | `uncalibrated`, `calibrating` or `running` |
| `cal` | Progress of a quick calibration, 0 to 1 |
| `motion`, `presence` | 0 or 1 |
| `source` | Where the active calibration came from: `wizard`, `quick`, `manual`, `none` |
| `saved` | 1 if it is stored in flash |
| `heap` | Free memory in bytes |
| `mqtt` | `disabled`, `connecting`, `connected` or `failed` |
| `wiz` | Guided calibration: `state` is `idle`, `waiting`, `countdown`, `recording`, `done` or `failed` |
| `res` | Result of the last guided calibration, once there is one |

## MQTT topics

With `<base>` = `ripple/ripple_<chip id>` (for example `ripple/ripple_4f7ddc`):

| Topic | Direction | Payload |
|---|---|---|
| `<base>/status` | board → | `online` / `offline` (retained; `offline` is also the Last Will) |
| `<base>/detecting` | board → | `online` when calibrated and detecting, else `offline` (retained) |
| `<base>/motion` | board → | `ON` / `OFF` (retained) |
| `<base>/presence` | board → | `ON` / `OFF` (retained) |
| `<base>/rssi` | board → | dBm, every 10 s |
| `<base>/fluctuation` | board → | peak variance since the last report, every 10 s |
| `<base>/threshold` | board → | current threshold (retained) |
| `<base>/threshold/set` | → board | a number from 0.5 to 100 |
| `<base>/calibrate/set` | → board | `PRESS` starts a quick calibration |

Home Assistant discovery messages go to
`<prefix>/<component>/ripple_<chip id>/<object>/config` (retained), with
`<prefix>` defaulting to `homeassistant`.

## Serial commands

At 115200 baud, typed with no Enter needed:

| Key | Action |
|---|---|
| `w` | Start the guided calibration |
| `n` | "I'm in position, start" for the current step |
| `x` | Cancel the guided calibration |
| `c` | Quick calibration (empty room) |
| `r` | Forget the saved calibration |
| `p` | Open the WiFi setup network |
| `W` | Development aid: join the build-time default network through the setup path |
