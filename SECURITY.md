# Security

## Supported versions

Only the latest release receives fixes.

## Reporting a vulnerability

Please **do not open a public issue** for a security problem. Report it privately
through GitHub: open the repository's **Security** tab and choose **Report a
vulnerability**. Include what you found, how to reproduce it and, if you can, what
you would suggest. You will get an answer within a few days.

## What to know about the current design

Ripple is a hobby device for trusted home networks. Be aware of these limits:

- **No authentication.** The dashboard, the REST API and the WebSocket are open to
  anyone on the same network, who can change settings (including MQTT and WiFi
  reset). **Do not expose the board to the internet** and do not port-forward it.
  A dashboard password is planned.
- **Open setup network.** During first-time setup the board's own network
  (`Ripple-xxxxxx`) has no password and the WiFi password you enter travels over
  plain HTTP on that network. Do the setup at home; the network closes as soon as
  setup succeeds.
- **Credentials in flash.** The WiFi and MQTT passwords are stored in plain text in
  the board's flash, like on most ESP8266 projects. Anyone with physical access to
  the board can read them. Use a dedicated MQTT account with minimal permissions.
  The API never returns the stored passwords.
- **Unsigned firmware.** Updates are done over USB; signed over-the-air updates are
  planned. Check release files against `SHA256SUMS`.
- **Privacy.** The sensor detects movement and cannot identify people, but it does
  reveal whether a room is occupied. Think about who can read your MQTT data.
