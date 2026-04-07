# ICP Monitor — Intracompartmental Pressure Monitoring Device

A wireless pressure monitoring system for intracompartmental pressure (ICP) measurement built for MIT 2.75 Medical Device Design. The device reads pressure via I2C from a Honeywell ABP2 sensor, filters the signal, and streams data in real time to a web dashboard served directly from the ESP32 over WiFi. No app installation is required — any device with a browser can connect.

---

## Hardware

### Components

| Component | Part Number | Notes |
|-----------|------------|-------|
| Microcontroller | Seeed Studio XIAO ESP32-C5 | RISC-V, WiFi 6, BLE 5.0, built-in LiPo charging |
| Pressure Sensor | Honeywell ABP2LANT001PG2A3XX | 0–1 PSI gauge, I2C, 24-bit output |
| Battery | 3.7V LiPo (optional) | Connect to B+/B− pads on board underside |

### Wiring

```
XIAO ESP32-C5          Honeywell ABP2
─────────────          ──────────────
3.3V          ──────►  VDD
GND           ──────►  GND
D4 (SDA)      ──────►  SDA
D5 (SCL)      ──────►  SCL
```

### Power

- **USB-C**: Powers the board and charges the LiPo battery simultaneously via the onboard SGM40567 charge management IC (100 mA charge current).
- **Battery only**: Connect a 3.7V LiPo to the B+/B− pads. The board boots automatically on power application — no button press needed.
- **RESET button**: Restarts firmware. **BOOT button**: Hold while pressing RESET only when reflashing a bricked device.

---

## Software Architecture

### Project Structure

```
ICP_Monitor/
├── ICP_Monitor.ino     Main firmware — setup/loop, orchestration
├── config.h            All pin definitions, constants, and tunable parameters
├── sensor.h/.cpp       Pressure sensor I2C driver
├── filter.h/.cpp       Rolling average + spike rejection filter
├── battery.h/.cpp      Battery voltage monitoring
├── webserver.h/.cpp    WiFi AP, HTTP server, WebSocket, captive portal
└── dashboard.h         Complete web UI embedded as a PROGMEM string literal
```

### Data Flow

```
Honeywell ABP2 (I2C)
        │  7-byte response @ 10 Hz
        ▼
  sensor.cpp
  sensorRead()          → raw 24-bit pressure counts + temperature counts
        │
        ▼  Transfer function → PSI → mmHg − zero offset
  filter.cpp
  filterSample()        → rolling average + spike rejection
        │
        ▼
  ICP_Monitor.ino       → broadcast JSON via WebSocket @ 10 Hz
        │
        ▼
  webserver.cpp
  ws.textAll(json)      → push to all connected browser clients
        │
        ▼
  dashboard.h (JS)      → update live display + chart + alert check
```

### Module Descriptions

#### `config.h`
Central configuration header. All tunable parameters live here — no magic numbers elsewhere.

Key constants:

| Constant | Default | Description |
|----------|---------|-------------|
| `SENSOR_INTERVAL_MS` | 100 | Sensor poll period (ms). 100 = 10 Hz |
| `FILTER_WINDOW` | 10 | Rolling average window in samples (= 1 s at 10 Hz) |
| `SPIKE_THRESHOLD_MMHG` | 15.0 | Max allowed single-sample jump (mmHg) before rejection |
| `AP_SSID` | "ICP-Monitor" | WiFi network name |
| `AP_PASSWORD` | "pressure" | WiFi password |
| `BAT_VOLT_FULL` | 4.2 V | LiPo full voltage |
| `BAT_VOLT_EMPTY` | 3.0 V | LiPo empty voltage |

#### `sensor.h/.cpp`
Encapsulates all communication with the Honeywell ABP2 sensor.

**Protocol**: I2C at address `0x28`. Each measurement cycle:
1. Write 3-byte command `[0xAA, 0x00, 0x00]` to trigger a conversion.
2. Wait 10 ms for the sensor ASIC to complete the measurement.
3. Read 7 bytes: `[status, P[23:16], P[15:8], P[7:0], T[23:16], T[15:8], T[7:0]]`.
4. Validate status byte: `0x40` = powered on and not busy. Any other value = error or busy.

**Transfer function** (Honeywell ABP2 datasheet, 10%–90% calibration):
```
pressure_psi = ((counts − OUT_MIN) × (P_MAX − P_MIN)) / (OUT_MAX − OUT_MIN) + P_MIN

where:
  OUT_MIN = 1,677,722   (10% × 2^24)
  OUT_MAX = 15,099,494  (90% × 2^24)
  P_MIN   = 0.0 psi
  P_MAX   = 1.0 psi
```

**Temperature** (standard ABP2 formula):
```
temp_c = (counts × 200) / 16,777,215 − 50
```

**Return values** (`SensorResult` enum):

| Value | Meaning |
|-------|---------|
| `SENSOR_OK` | Valid reading |
| `SENSOR_ERROR` | I2C failure or bad status byte |
| `SENSOR_OVERRANGE` | Pressure counts ≥ `OUT_MAX` (above 1 PSI = 51.7 mmHg) |
| `SENSOR_UNDERRANGE` | Pressure counts ≤ `OUT_MIN` (below 0 PSI) |

**Unit conversion**: `1 PSI = 51.7149 mmHg`

#### `filter.h/.cpp`
Two-stage signal conditioning applied to every valid pressure reading.

**Stage 1 — Spike Rejection**

Before updating the rolling average, the incoming sample is compared against the current average:
```
if |newSample − currentAverage| > SPIKE_THRESHOLD_MMHG:
    discard newSample, return currentAverage unchanged
```
This prevents electrical transients or I2C glitches from corrupting the display. The threshold (15 mmHg default) is configurable in `config.h`.

**Stage 2 — Rolling Average**

A circular buffer of `FILTER_WINDOW` samples is maintained. The output is the mean of all samples currently in the buffer:
```
output = sum(buf[0..N−1]) / N
```
At 10 Hz with a window of 10 samples, this produces a 1-second moving average, suppressing physiological noise and sensor quantization noise without introducing clinically meaningful lag.

`filterReady()` returns false until the buffer is at least half full, preventing the display of misleadingly smooth values during the first 500 ms of operation.

The filter is reset (`filterInit()`) when a zero/tare command is issued, ensuring the new baseline is established from fresh samples only.

#### `battery.h/.cpp`
Reads battery voltage from the ADC pin on the XIAO ESP32-C5.

The board contains a 1:2 resistor voltage divider (two 100 kΩ resistors) connecting the battery positive terminal to the ADC input. 16 ADC samples are averaged using `analogReadMilliVolts()` for stability, then the result is multiplied by 2 to recover the actual battery voltage.

Battery percentage is derived from a linear interpolation between 3.0 V (0%) and 4.2 V (100%). If the measured voltage is below 2.5 V (indicating no battery is connected and the pin is floating), `batteryPercent()` returns `−1` and the dashboard displays "USB" instead of a percentage.

Battery is sampled every 5 seconds (`BATTERY_INTERVAL_MS`) since it changes slowly and ADC sampling introduces a brief interrupt in the main loop.

#### `webserver.h/.cpp`
Manages the WiFi access point, HTTP server, WebSocket server, and captive portal.

**WiFi Access Point**: The ESP32 creates a standalone WiFi network (`WIFI_AP` mode). No external router or internet connection is required. The default IP is `192.168.4.1`.

**Captive Portal**: Multiple platform-specific captive portal detection endpoints are served to prevent phones from marking the network as "no internet" and auto-disconnecting:

| Endpoint | Platform |
|----------|----------|
| `/hotspot-detect.html` | iOS / macOS |
| `/generate_204` | Android |
| `/connecttest.txt` | Windows |

All other unrecognized paths redirect to `http://192.168.4.1/`.

A DNS server (`DNSServer` library) resolves all DNS queries to the ESP32's own IP, completing the captive portal behavior.

**WebSocket** (`AsyncWebSocket` at `/ws`):

Incoming messages (client → server) are parsed as JSON:

| Command | Action |
|---------|--------|
| `{"cmd":"zero"}` | Sets current raw pressure as the zero offset, resets filter |

Outgoing messages (server → client) are broadcast to all connected clients at 10 Hz:

```json
{
  "p":   25.3,    // Filtered pressure in mmHg (float, 1 decimal place)
  "t":   36.8,    // Sensor temperature in °C
  "bat": 78,      // Battery percentage (0–100, or -1 for USB-only)
  "chg": false,   // Charging indicator (heuristic: voltage > 4.15 V)
  "ts":  123456,  // millis() timestamp
  "ok":  true,    // Sensor read succeeded
  "ovr": false    // Sensor overrange flag
}
```

Short keys are used to minimize WebSocket frame size.

#### `dashboard.h`
The complete web interface — HTML, CSS, and JavaScript — stored as a single `PROGMEM` raw string literal and served from flash memory. This avoids the need for a LittleFS filesystem partition and a separate file upload step.

**Key JavaScript components**:

- **WebSocket client**: Connects to `ws://<host>/ws`, auto-reconnects after 2 s on disconnect.
- **Chart**: Custom scrolling line chart implemented with the Canvas 2D API. Maintains a 600-sample circular buffer (60 seconds at 10 Hz). Draws grid lines, a dashed red threshold line, the pressure trace, and a translucent fill. No external charting libraries are used (eliminating the CDN dependency that would not be available on the isolated AP network).
- **Alert system**: When pressure ≥ threshold, the display flashes red and a 880 Hz sine wave beep is generated via the Web Audio API. Beeps are rate-limited to one every 2 seconds.
- **iOS audio unlock**: The Web Audio API requires a user gesture before audio can be played on iOS Safari. A "Start Monitoring" button is shown as a modal overlay on first load; tapping it initializes the `AudioContext` and dismisses the overlay.
- **Threshold persistence**: The alert threshold is stored in `localStorage` so it survives page refreshes without requiring a round-trip to the ESP32.

---

## Getting Started

### Prerequisites

- Arduino IDE 2.x
- Seeed ESP32 board package (add `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json` to Additional Board Manager URLs)
- XIAO ESP32-C5 selected as the board target

### Library Installation

Install the following via **Sketch → Include Library → Manage Libraries**:

| Library | Author | Notes |
|---------|--------|-------|
| ArduinoJson | Benoit Blanchon | Version 7.x |
| ESPAsyncWebServer | Mathieu Carbou | Required for ESP32 Arduino core 3.x compatibility |
| AsyncTCP | Mathieu Carbou | Installed as dependency of ESPAsyncWebServer |

> **Important**: Use the Mathieu Carbou forks of ESPAsyncWebServer and AsyncTCP. The older me-no-dev versions are incompatible with ESP32 Arduino core 3.x and will fail to compile.

### Flashing

1. Connect the XIAO ESP32-C5 to your computer via USB-C.
2. Open `ICP_Monitor/ICP_Monitor.ino` in Arduino IDE.
3. Select **Tools → Board → XIAO_ESP32C5**.
4. Select the correct COM port under **Tools → Port**.
5. Click **Upload**.

To verify operation, open **Tools → Serial Monitor** at 115200 baud and press the RESET button. Expected output:
```
=== ICP Monitor ===
Sensor initialized (I2C)
[Battery] Raw ADC on A0: 1850 mV  (x2 = 3.70 V)
Battery monitor initialized
AP started. IP: 192.168.4.1
Web server started on port 80
Ready. Connect to WiFi: ICP-Monitor
```

### Connecting

1. On iPhone/laptop: **Settings → WiFi → ICP-Monitor** (password: `pressure`)
2. Safari/browser opens automatically via captive portal, or navigate to `http://192.168.4.1`
3. Tap **Start Monitoring** to enable audio alerts
4. Live pressure readings begin immediately

---

## Using the Dashboard

### Zeroing (Tare)

Before measurement, expose the sensor to atmospheric pressure (needle not yet inserted) and tap the **ZERO** button. The current reading is stored as the offset; all subsequent readings are displayed relative to zero. A "(zeroed)" confirmation appears briefly on the chart.

The zero offset is held in RAM and resets when the device is power-cycled. Re-zero at the start of each procedure.

### Alert Threshold

The default threshold is **30 mmHg**, the standard clinical criterion for compartment syndrome requiring fasciotomy. Adjust with the +/− buttons or by typing directly in the field. The threshold is saved to browser `localStorage` and persists across page refreshes.

When pressure exceeds the threshold:
- The pressure display turns red
- The status badge pulses with "ALERT — HIGH PRESSURE"
- A 880 Hz beep sounds every 2 seconds (requires "Start Monitoring" to have been tapped)

### Overrange

If the sensor's output saturates (pressure above 1 PSI / 51.7 mmHg), the display shows **"OVR"** in orange. This indicates the physical pressure exceeds the sensor's measurement range. The chart pauses during overrange conditions.

---

## Signal Processing Detail

Raw sensor readings at 10 Hz pass through a median filter before display:

```
Raw ADC counts → PSI → mmHg → subtract zero offset
                                     │
                              Median Filter
                              (5-sample window, 500 ms at 10 Hz)
                                     │
                              Displayed value
```

**How the median filter works**: A circular buffer of the 5 most recent samples is maintained. On each new sample, a sorted copy of the buffer is produced (insertion sort, ≤10 comparisons) and the middle element is returned as the output.

**Why median over alternatives**:
- A single spike can only become the median if it is 3 or more of the last 5 samples — a single bad I2C read is invisible to the output.
- Unlike threshold-based spike rejection, there is no parameter to tune and no risk of suppressing a real rapid pressure change — if the pressure genuinely rises, the majority of the buffer rises with it and the median follows immediately.
- Unlike a rolling average, spikes do not bleed proportionally into the output; they are simply voted out.

To adjust the window size, modify `FILTER_WINDOW` in `config.h` (keep it odd) and re-flash.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| "Sensor not found" on Serial | Wiring issue or missing pull-ups | Check SDA→D4, SCL→D5; add 4.7 kΩ pull-ups |
| Pressure reads ~−26 mmHg (not zeroed) | Atmospheric offset not cleared | Tap ZERO button before measurement |
| Battery shows "USB" | No LiPo connected | Connect 3.7V LiPo to B+/B− pads |
| Battery shows 0% | Wrong ADC pin or no divider | Check raw ADC value in Serial Monitor at startup |
| Dashboard won't load | Phone dropped AP WiFi | Reconnect to "ICP-Monitor" in WiFi settings |
| No beep on alert | iOS audio not unlocked | Ensure "Start Monitoring" was tapped |
| Compilation error: `HTTP_GET` not declared | Wrong ESPAsyncWebServer version | Delete old library, install Mathieu Carbou fork |

---

## Configuration Reference

All parameters are in `ICP_Monitor/config.h`:

```cpp
// I2C
#define SDA_PIN              D4
#define SCL_PIN              D5
#define I2C_ADDR             0x28

// Sensor transfer function (do not modify — matches ABP2 datasheet)
const float P_MIN   = 0.0;
const float P_MAX   = 1.0;
const float OUT_MIN = 1677722.0;
const float OUT_MAX = 15099494.0;
const float PSI_TO_MMHG = 51.7149;

// Filter
#define FILTER_WINDOW        10      // samples (1 s at 10 Hz)
#define SPIKE_THRESHOLD_MMHG 15.0f  // mmHg

// Timing
#define SENSOR_INTERVAL_MS   100    // 10 Hz
#define BATTERY_INTERVAL_MS  5000   // 0.2 Hz

// WiFi
#define AP_SSID     "ICP-Monitor"
#define AP_PASSWORD "pressure"
```
