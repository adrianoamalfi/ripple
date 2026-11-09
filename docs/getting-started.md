# Getting started

This takes about ten minutes: flash the board, connect it to your WiFi, calibrate.

## What you need

- An ESP8266 board with **4 MB of flash**: NodeMCU v2/v3, Wemos/LOLIN D1 mini, or a
  generic ESP-12E/F module.
- A USB cable that carries data (some are charge-only) and a USB charger to power
  the board afterwards.
- A 2.4 GHz WiFi network. The ESP8266 cannot use 5 GHz.

## 1. Flash the firmware

### Option A: from your browser (recommended)

1. Open the [web installer](https://adrianoamalfi.github.io/ripple/install/) in
   **Chrome or Edge** on a computer (Safari and Firefox can't talk to USB serial
   devices).
2. Plug the board into the computer, choose your board model and press **Install**.
3. Pick the serial port that appears and follow the prompts. Erasing the flash
   when asked is fine on a first install.

### Option B: build it yourself

Install [PlatformIO](https://platformio.org/install), then:

```bash
git clone https://github.com/adrianoamalfi/ripple.git
cd ripple
pio run -e nodemcuv2 -t upload     # or d1_mini / esp12e
```

You do not need Node.js: the dashboard is already built into
`firmware_assets/dashboard.html.gz`. See [development](development.md) if you want
to change it.

### Option C: download a release

Grab `ripple-<board>-<version>.bin` from the
[releases page](https://github.com/adrianoamalfi/ripple/releases) and flash it at
offset `0x0` with `esptool.py write_flash 0x0 ripple-<board>-<version>.bin`.
Check it against `SHA256SUMS` first.

## 2. Connect it to your WiFi

A new board knows no network, so it opens its own:

1. On your phone or laptop, join the WiFi network **`Ripple-xxxxxx`** (it has no
   password; `xxxxxx` is part of the board's id).
2. A setup page opens by itself. If it doesn't, browse to **http://192.168.4.1/**.
3. Choose your network, type its password and press **Connect**.
4. The board tries the network first and only saves it if it really connects, so a
   typo can't lock you out. On success the page shows the board's address.
5. Reconnect your phone to your usual WiFi and open
   **`http://ripple-xxxxxx.local/`** (or the IP address shown). The same address
   works from then on.

To change networks later, open the dashboard and use **Forget this WiFi network**
(on the Device card): the board restarts into setup mode. If your router is simply
off for a while (a power cut, for example) the board waits and reconnects by itself.

> The setup network is open so that phones can join it without fuss. Do the setup
> at home, not in a crowded place.

## 3. Place the board

Where the board sits matters more than anything else. See
[calibration](calibration.md#placing-the-board). In short: not right next to the
router, a few metres away, so that people walk through the line between the two.

## 4. Calibrate

Open the dashboard and press **Start guided calibration**. It asks you to leave the
room, walk around, sit still and leave again, about two minutes in total, and then
tells you how well it works in that spot. Details in
[calibration](calibration.md).

Before you calibrate, the board runs a quick automatic calibration (it assumes the
room is empty) so that it works at all.

## 5. Home Assistant (optional)

Enter your MQTT broker in the dashboard's **Home Assistant** card. The device shows
up in Home Assistant on its own. See [home-assistant.md](home-assistant.md).

## Updating

For now, update over USB with the web installer or `pio run -t upload`. Your WiFi,
calibration and MQTT settings are kept (they live in a separate part of the flash).
Signed over-the-air updates are planned.
