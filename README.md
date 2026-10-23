# Clock Control — GitHub Pages + MQTT

A static, responsive web interface for sending a message, an 8×8 LED pattern, and an immediate alarm-trigger command to an ESP32 through MQTT.

## MQTT payload

Published to the topic configured in the page (default `clock/control`).

Example:
```json
{
  "type": "clock_control",
  "trigger": true,
  "timestamp": "2026-10-23T09:00:00.000Z",
  "message": "Wake up!",
  "matrix": [
    "00111100",
    "01000010",
    "10100101",
    "10000001",
    "10100101",
    "10011001",
    "01000010",
    "00111100"
  ]
}
```
`message` is omitted if blank or disabled. `matrix` is omitted if its toggle is off. `trigger` is always present. Matrix strings are 8 rows, each containing 8 characters (`0`/`1`); left-to-right orientation may need to be reversed to match your physical module.

## GitHub Pages deployment

1. Create a GitHub repository, e.g. `clock-control`.
2. Upload `index.html` to the repository root.
3. Open **Settings → Pages**.
4. Choose **Deploy from a branch**, branch `main`, folder `/ (root)`, then Save.
5. Wait for GitHub Pages to publish the site.

## Configure the broker

- Enter the HiveMQ Cloud cluster hostname only, e.g. `your-cluster.s1.eu.hivemq.cloud` (no `https://`).
- Browser connection uses secure WebSockets, commonly port `8884`, path `/mqtt`.
- Use a dedicated MQTT user with the minimum permissions needed to publish only to `clock/control`.
- ESP32 subscribes to the same topic over MQTT/TLS, commonly port `8883`.

## Security notes

GitHub Pages is static hosting, so anything hardcoded in the page source is public. This page does not hardcode credentials or store them in local storage; credentials are entered in the browser at connection time. Anyone with access to the page can still enter their own broker credentials, and browser extensions / the device itself are outside this page's control. For a public deployment, use a backend or broker-supported per-user credentials/ACLs and rotate any credentials that may have been exposed. Do not reuse administrator credentials.

## ESP32 integration

See `esp32_mqtt_integration.ino`. It is an integration template, not a replacement for your existing clock sketch: merge the MQTT setup/loop calls and connect the marked callback functions to your actual alarm, OLED, and matrix code.
