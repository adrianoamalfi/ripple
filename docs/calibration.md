# Calibration and placement

## How detection works

Ripple measures the received signal strength (RSSI) of the link between the board
and your router about **10 times a second**. It keeps a sliding window of the last
**2 seconds** and computes the *variance* of the RSSI in that window: how much the
signal is jumping around. In a still room that number is small and steady. When
someone moves it rises.

**Motion** starts when that variance goes above a *threshold*, and ends when it
falls back below about half of the way down to the quiet level (a little
hysteresis, so it doesn't flicker). **Presence** is on while there is motion and
for **30 seconds** after the last motion. (An adjustable timeout is planned.)

The whole question is where to put the threshold: too low and ordinary signal noise
triggers it, too high and real movement is missed. Calibration answers that from
your own room.

## Guided calibration

Press **Start guided calibration** in the dashboard (or `w` on the serial port). It
has four steps. For each one, press **I'm in position, start**; you get **10
seconds** to get in position, then it records for **30 seconds**.

| Step | What you do | What it learns |
|---|---|---|
| 1. Empty room | Leave the room and close the door | The signal's normal noise |
| 2. Walk | Walk back and forth, between the board and the router | What motion looks like |
| 3. Sit still | Sit down and stay as still as you can | Whether a still person is visible |
| 4. Empty again | Leave again | A second look at the noise: a stability check |

The threshold is set just above the highest noise seen in the empty-room steps
(1.15 times the peak), so ordinary noise doesn't trigger it. Then the walking step
is replayed against that threshold: in what share of its seconds would motion have
been detected?

### Reading the result

| Quality | Meaning |
|---|---|
| **Good** | Walking would have been detected in at least 60% of its seconds, and the second empty step stayed quiet. |
| **Fair** | Walking is detected in 30–60% of its seconds: it works, but misses some movement. |
| **Poor** | Walking is detected in under 30% of its seconds: at this spot it can't be told apart from an empty room. |

Under the quality you also see *Sitting still detected*. That is usually close to
0%: a person who isn't moving doesn't disturb the signal, which is why presence is
"motion in the last 30 seconds" rather than "someone is here".

The result is saved on the board and restored after a reboot. Run the calibration
again whenever you move the board or rearrange the room.

### Quick calibration

The **Quick calibration** button (or `c`) listens to an *empty* room for about 45
seconds and sets the threshold from that alone. It's what runs by itself on a fresh
board. It can't check that walking is actually visible, so prefer the guided one.

### Setting the threshold by hand

The **Sensitivity** slider (or the *Motion threshold* number in Home Assistant)
moves the threshold. Lower is more sensitive; higher is more robust against false
alarms. Staying above the "empty-room noise peak" shown next to it avoids false
alarms. A manual threshold is saved too.

## Placing the board

The signal between the board and the router is shaped by walls, furniture and
distance. A few rules of thumb from testing:

- **Don't put it right next to the router.** At −40 to −50 dBm the link is so
  strong and stable that a person walking by barely changes it. A weaker signal,
  around **−60 to −70 dBm** (check the *Signal* tile), is much more sensitive.
- **Put people on the line between the two.** Movement across the straight path
  between board and router disturbs it far more than movement in a far corner.
- **Keep it away from big metal objects** and from things that move on their own
  (fans, curtains in a draft).
- **One room per board.** Walls attenuate the effect; a board in the corridor won't
  see much of what happens behind a closed door.
- **Try, calibrate, look at the quality, move, repeat.** The wizard exists exactly
  for this.

If the quality stays **Poor** in every spot you try, the limit is the ESP8266's
integer RSSI rather than your setup. Please open an issue with your room layout and
the result; real-world reports help a lot.
