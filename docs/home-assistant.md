# Home Assistant

Ripple talks to Home Assistant over **MQTT** and uses **MQTT discovery**, so there
is nothing to write in YAML: the device and its entities appear by themselves.

## Prerequisites

An MQTT broker that Home Assistant uses, for example the **Mosquitto broker**
add-on, and the **MQTT** integration set up in Home Assistant.

## Connect the board

1. Open the dashboard (`http://ripple-xxxxxx.local/`) and find the **Home
   Assistant** card.
2. Enter the broker's **IP address** (names ending in `.local` aren't resolved) and
   the port (usually `1883`).
3. If your broker needs a login, enter the username and password. Create a
   dedicated broker user for Ripple with limited permissions: the password is kept
   in plain text in the board's flash (see [Security](../SECURITY.md)).
4. Switch **Enable MQTT** on and press **Save**.

The badge in the card turns to **Connected**. If it says *Failed* you see why:
*Broker unreachable*, *Wrong user or password*, and so on. The board keeps retrying
with a growing delay (5 s up to 60 s) and never lets a broker problem interrupt the
sensing.

Leave the **discovery prefix** at `homeassistant` unless you changed it in Home
Assistant's MQTT integration.

## What you get

In Home Assistant, go to **Settings → Devices & services → MQTT**: there is a device
named **Ripple xxxxxx** with these entities.

| Entity | Type | Notes |
|---|---|---|
| Motion | Binary sensor (`motion`) | On while the signal moves above the threshold |
| Presence | Binary sensor (`occupancy`) | On during motion and for 30 s after it |
| Signal strength | Sensor (dBm, diagnostic) | RSSI of the link to your router |
| Signal fluctuation | Sensor (dB², diagnostic) | Peak window variance since the last update |
| Motion threshold | Number (config) | Read and change the sensitivity from Home Assistant |
| Quick calibration | Button (config) | Starts a quick calibration. **Empty the room first.** |

**Availability.** Motion, presence and the threshold show as *unavailable* until the
board is calibrated, and everything goes *unavailable* if the board drops off the
network (MQTT Last Will), so an offline sensor never looks like "no motion".

Signal and fluctuation are sent every 10 seconds; motion and presence the moment
they change, and are retained so Home Assistant has them after a restart.

## Example automation

Turn a light on for motion and off a minute after the room has been quiet:

```yaml
automation:
  - alias: Hallway light on motion
    triggers:
      - trigger: state
        entity_id: binary_sensor.ripple_xxxxxx_motion
        to: "on"
    actions:
      - action: light.turn_on
        target:
          entity_id: light.hallway
  - alias: Hallway light off when quiet
    triggers:
      - trigger: state
        entity_id: binary_sensor.ripple_xxxxxx_presence
        to: "off"
        for: "00:01:00"
    actions:
      - action: light.turn_off
        target:
          entity_id: light.hallway
```

Use the *presence* sensor to turn things off (it already waits 30 s after the last
movement) and the *motion* sensor to turn them on.

## Topics

Everything lives under `ripple/ripple_<chip id>/`. The full list is in the
[API reference](api.md#mqtt-topics).

## Trying it without Home Assistant

Install Mosquitto (`brew install mosquitto`), run it with a config containing
`listener 1883` and `allow_anonymous true`, point the board at your computer's IP
and watch everything with `mosquitto_sub -v -t '#'`.
