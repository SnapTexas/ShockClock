# ShockClock
<p align="center">
  <img src="assets/shockclock.jpg" alt="ShockClock - ESP32 Alarm Clock" width="600">
</p>

<p align="center">
  An ESP32-powered alarm clock with OLED display, LED matrix animations, and web-based controls.
</p>
**Your Clock. Your Rules.**

ShockClock is an experimental ESP32-based smart alarm clock that combines embedded systems, hardware prototyping, web technologies, and MQTT communication.

The project explores building an interactive alarm clock with a physical interface, animated LED matrix, OLED display, and remote triggering through a browser-based control panel.

Currently under development, ShockClock is an ongoing project where hardware, firmware, and web functionality are being developed and refined.

## Project Features

### Hardware & Embedded Systems
- **ESP32:** Main microcontroller responsible for system operations.
- **RTC Timekeeping:** DS3231 module for real-time clock functionality.
- **OLED Interface:** SSD1306 128×64 display for time and menu navigation.
- **LED Matrix Animation:** MAX7219 8×8 matrix displaying animated skull expressions.
- **Rotary Encoder:** Physical interface for menu navigation and alarm configuration.
- **Persistent Storage:** ESP32 Preferences for saving alarm settings.
- **Speaker Interface:** GPIO-controlled speaker activation sequence.
- **Relay Interface:** Hardware control output.

### Connectivity & Web
- Wi-Fi connectivity through ESP32.
- MQTT communication using HiveMQ.
- Browser-based remote control interface.
- Remote alarm triggering through MQTT messages.
- Connection status monitoring.
- Automatic MQTT reconnection on the web client.
- Interactive button animations and sound effects.

## System Architecture

ShockClock consists of three main components:

1. **ESP32 Firmware:** Handles clock functionality, hardware peripherals, alarm configuration, and MQTT communication.
2. **MQTT Communication:** Connects the ESP32 and browser-based control interface through HiveMQ.
3. **Web Control Panel:** Provides a browser interface for remotely triggering the clock.

```text
             WEB CONTROL PANEL
                    |
                    |
              MQTT over WSS
                    |
                    v
              HIVEMQ BROKER
                    |
                    |
              MQTT over TLS
                    |
                    v
                ESP32
                    |
          +---------+---------+
          |         |         |
          v         v         v
         OLED    LED Matrix  Speaker
          |
          v
       DS3231 RTC

       ESP32 GPIO
           |
           v
       Relay Interface
```

## Web Control Panel

ShockClock includes a browser-based remote control interface built using HTML, CSS, and JavaScript.

The interface communicates with the ESP32 through HiveMQ using MQTT over WebSockets.

### Interface Features

- Minimal dark-themed interface.
- Large interactive alarm trigger button.
- Real-time MQTT connection indicator.
- Button press and rebound animations.
- Synthesized button sound effect using Web Audio API.
- Interactive popup messages.
- Sequential humorous messages displayed after button presses.
- Connection status and publishing feedback.
- Automatic reconnection handling.

### Remote Trigger Workflow

The web interface publishes an MQTT message when the user presses the trigger button and closes the popup.

**MQTT Broker:** HiveMQ Public Broker

**WebSocket Endpoint:**
```text
wss://broker.hivemq.com:8884/mqtt
```

**MQTT Topic:**
```text
snapclock-00989800/control
```

**Payload:**
```json
{
  "trigger": true
}
```

The ESP32 subscribes to the same topic using MQTT over TLS on port 8883.

When the message is received, the ESP32 activates its remote alarm display state.

The remote trigger currently controls the alarm display and LED matrix animation, independently of the saved RTC alarm.

## Hardware Components

| Component | Purpose |
|---|---|
| ESP32 DevKit | Main microcontroller |
| DS3231 | Real-time clock |
| SSD1306 128×64 | OLED display |
| MAX7219 8×8 | LED matrix animation |
| Rotary Encoder | Physical menu navigation |
| Relay Module | Hardware control interface |
| Speaker / Sound Module | Audio output |
| Power Supply | System power |

## Pin Configuration

| Component | ESP32 GPIO |
|---|---:|
| Speaker Control | 18 |
| Music Control | 5 |
| Relay | 23 |
| OLED SDA | 21 |
| OLED SCL | 22 |
| Encoder Button | 32 |
| Encoder CLK | 33 |
| Matrix DIN | 13 |
| Matrix CLK | 27 |
| Matrix CS | 14 |

Both the DS3231 and SSD1306 use the I2C interface.

## OLED Menu System

The firmware currently contains a physical menu system controlled using the rotary encoder.

### Main Menu
- Alarm
- Mode
- Back

### Alarm Menu
- Hour
- Minute
- Set
- Delete
- Back

### Mode Menu
- Normal
- Safe
- Crazy
- Back

Alarm configuration is stored using ESP32 Preferences.

## Operating Modes — Planned

The firmware contains preliminary mode definitions and a permission configuration map. However, **operating modes are not yet fully implemented**.

The following modes are planned:

| Mode | Code | Intended Purpose |
|---|---|---|
| Normal | N | Standard clock operation |
| Safe | S | Restricted functionality |
| Crazy | C | Expanded functionality and remote access |
| Music | M | Music-oriented operation |

The permission configuration currently exists in the source code, but mode-dependent behavior has not yet been integrated throughout the firmware.

Music mode is also not currently selectable through the OLED menu.

These modes should be considered planned functionality rather than completed features.

## LED Matrix Animation

The MAX7219 8×8 LED matrix displays a skull animation using predefined bitmap frames.

Each frame has an individual display duration.

Animation updates use `millis()` to avoid relying on blocking delays during normal operation.

## Software Dependencies

### ESP32 Firmware

| Library | Purpose |
|---|---|
| Arduino Core for ESP32 | Microcontroller development |
| Preferences | Persistent storage |
| U8g2 | OLED graphics |
| RTClib | RTC communication |
| PubSubClient | MQTT communication |
| MD_MAX72XX | LED matrix control |
| WiFi | Wireless connectivity |
| WiFiClientSecure | Secure MQTT transport |

### Web Interface

| Technology | Purpose |
|---|---|
| HTML5 | Interface structure |
| CSS3 | Styling and animations |
| JavaScript | Interface logic |
| MQTT.js | Browser MQTT communication |
| Web Audio API | Button sound effects |
| HiveMQ | MQTT broker |

## Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/SnapTexas/ShockClock.git
```

### 2. Configure the ESP32

Open `ShockClock.ino` in Arduino IDE.

Install the required libraries and select your ESP32 board.

### 3. Configure Wi-Fi

Update the following definitions:

```cpp
const char* WIFI_SSID = "Your";
const char* WIFI_PASSWORD = "12345678";
```

Use your own network credentials. Do not commit actual credentials to a public repository.

### 4. Configure MQTT

The current firmware and web interface use the HiveMQ public broker.

Ensure that both components use the same MQTT topic.

### 5. Upload the firmware

Connect your ESP32 and upload the sketch.

Open Serial Monitor at 115200 baud to inspect initialization and connection messages.

### 6. Open the Web Interface

Open the web control page through your hosted web interface or locally in a browser.

Once the MQTT connection is established, the trigger button becomes available.

## Current Development Status

ShockClock is still under active development. The current implementation contains working components alongside unfinished functionality and known bugs.

| Component | Status |
|---|---|
| ESP32 initialization | Implemented |
| RTC timekeeping | Implemented |
| OLED clock display | Implemented |
| OLED menu navigation | Partially implemented |
| Alarm configuration | Implemented |
| Persistent alarm storage | Implemented |
| LED matrix skull animation | Implemented |
| Wi-Fi connectivity | Implemented |
| MQTT communication | Implemented |
| Browser-based control panel | Implemented |
| Remote alarm triggering | Partially implemented |
| Speaker control sequence | Partially implemented |
| Operating modes | Not implemented |
| Music mode | Planned |
| Encoder direction detection | Incomplete |
| Remote alarm timeout | Incomplete |
| Power-saving functionality | Planned |

### Known Issues & Limitations

Some known limitations in the current implementation:

- Rotary encoder navigation currently supports forward-only movement because direction detection is not implemented.
- The encoder DT input is not configured.
- Operating mode permissions are defined but not enforced.
- The remote alarm duration is defined but automatic expiration is not implemented in the firmware.
- The alarm matching logic uses a five-minute window and needs refinement.
- Some menu and alarm interactions still require debugging.
- Speaker activation and deactivation sequences need further testing.
- MQTT security and connection handling need further hardening.

This list is not exhaustive; additional bugs may be discovered during hardware testing.

## Future Improvements

- Complete operating mode implementation.
- Improve rotary encoder navigation.
- Fix existing menu and alarm-related bugs.
- Implement remote alarm timeout handling.
- Add MQTT authentication and proper TLS certificate verification.
- Integrate mode-specific permissions.
- Complete Music mode.
- Improve speaker control and sound functionality.
- Implement power-saving functionality.
- Improve web-to-device communication.
- Add more customizable animations and interface features.

## Safety & Security

ShockClock is an experimental hardware project.

The relay interface is capable of changing its GPIO output state. Verify the circuit and use safe, isolated test loads.

The current MQTT configuration uses a public broker and a publicly known topic. Messages are not authenticated by the application. The firmware also currently uses `setInsecure()`, which disables TLS certificate verification.

Do not use the current configuration for security-sensitive or hazardous hardware applications.

## Technologies Used

`ESP32` · `Arduino C++` · `HTML` · `CSS` · `JavaScript` · `MQTT` · `HiveMQ` · `I2C` · `DS3231` · `SSD1306` · `MAX7219`

## Author

**Shahid Shaikh (SnapTexas)**

- GitHub: [@SnapTexas](https://github.com/SnapTexas)
- Repository: [ShockClock](https://github.com/SnapTexas/ShockClock)

---

*Your Clock. Your Rules.*
