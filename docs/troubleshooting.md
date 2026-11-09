# Troubleshooting

## The setup network `Ripple-xxxxxx` doesn't appear

- It only opens when the board has no saved network or can't reach it. Give a
  powered board 30 seconds after boot.
- It is a 2.4 GHz network; some phones hide those if they are set to 5 GHz only.
- To force it from the dashboard use **Forget this WiFi network**; over USB, send
  `p` on the serial port.

## The setup page doesn't open by itself

Browse to **http://192.168.4.1/** while you are connected to `Ripple-xxxxxx`. Some
phones skip the automatic page when they find the network has no internet; turn
off mobile data and try again, or open the address by hand.

## "Could not connect to that network"

Check the password (it is case-sensitive) and that the network is 2.4 GHz and in
range of the board. The board then returns to its previous network, so nothing is
lost. The serial log (115200 baud) shows the reason.

## I can't find the board after setup

Open **`http://ripple-xxxxxx.local/`**, or look for the board in your router's list
of devices. `.local` names need mDNS: they work on macOS, iOS, recent Android and
Windows 10+; on some Linux setups you need `avahi`. The serial log prints the
address at boot.

## The dashboard says "Reconnecting" or loses the board now and then

The board answers a handful of clients at once. Close extra tabs. If it happens
constantly, the WiFi link is weak: check the *Signal* tile and move the board or the
router.

## Motion is never detected

Look at the **calibration result**. If it says *Poor*, walking can't be told apart
from an empty room at this spot: move the board farther from the router so the
signal is weaker (−60 to −70 dBm) and put the walking path between the two. See
[calibration](calibration.md#placing-the-board).

## It reports motion all the time

The threshold is too low for the noise, or something nearby moves (a fan, a pet,
curtains). Raise the **Sensitivity** slider above the empty-room noise peak, or run
the guided calibration again with the room really empty.

## Home Assistant doesn't show the device

- The dashboard's **Home Assistant** card must say **Connected**. If it says
  *Failed*, the message says why.
- Use the broker's IP address, not a `.local` name.
- The MQTT integration in Home Assistant must be set up, with the same broker.
- The discovery prefix must match Home Assistant's (default `homeassistant`).
- Watch the traffic: `mosquitto_sub -h <broker> -v -t 'ripple/#' -t 'homeassistant/#'`.

## The board restarts by itself

Send nothing and watch the serial log at boot: it prints `Reset reason:`. An
`Exception` means a crash: please open an issue with the log. A power-related reset
usually means a weak USB cable or charger.

## Flashing fails

Use a USB cable that carries data, try another port, and hold the board's FLASH/BOOT
button while pressing RESET if the installer can't connect. Make sure the board is
selected correctly in the installer.

## Start over

Use **Forget this WiFi network** and **Reset calibration** in the dashboard. To
erase everything, flash with the web installer and choose to erase the flash.
