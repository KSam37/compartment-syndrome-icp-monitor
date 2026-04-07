# ICP Monitor — Intracompartmental Pressure Monitoring Device

A wireless pressure monitoring system for intracompartmental pressure (ICP) measurement built for MIT 2.75 Medical Device Design. The device reads pressure from a Honeywell ABP2 sensor via I2C, filters the signal, and streams data in real time to a web dashboard served directly from the ESP32 over its own WiFi network. No app installation is required — any device with a browser (iPhone, Android, laptop) can connect.

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

> The ABP2 requires I2C pull-up resistors. If your breakout does not include them, add 4.7 kΩ resistors from SDA→3.3V and SCL→3.3V.

### Power

- **USB-C**: Powers the board and charges the LiPo simultaneously via the onboard SGM40567 charge IC (100 mA charge current).
- **Battery only**: Connect a 3.7V LiPo to the B+/B− pads on the underside. The board boots automatically on power-up — no button press needed.
- **RESET button**: Restarts firmware. **BOOT button**: Hold while pressing RESET only when reflashing a bricked device.

---

## Software Architecture

### Project Structure

```
ICP_Monitor/
├── ICP_Monitor.ino     Main firmware — setup/loop, orchestration
├── config.h            Pin definitions, constants, and default parameters
├── sensor.h/.cpp       Honeywell ABP2 I2C driver
├── filter.h/.cpp       5-sample median filter (runtime-adjustable window)
├── battery.h/.cpp      Battery voltage monitoring
├── webserver.h/.cpp    WiFi AP, HTTP server, WebSocket, captive portal
└── dashboard.h         Complete web UI embedded as a PROGMEM string literal
```

### Data Flow

```
Honeywell ABP2 (I2C)
        │  7-byte response @ configurable rate (default 10 Hz)
        ▼
  sensor.cpp
  sensorRead()          → raw 24-bit counts, SensorResult enum
        │
        ▼  Transfer function → PSI → mmHg − zero offset
  filter.cpp
  filterSample()        → median of last N samples (default N=5)
        │
        ▼
  ICP_Monitor.ino       → build JSON, broadcast via WebSocket
        │
        ▼
  webserver.cpp
  ws.textAll(json)      → push to all connected browser clients
        │
        ▼
  dashboard.h (JS)      → update display, chart, alerts, prototyping panel
```

---

## Module Descriptions

### `config.h`
Central configuration header. All tunable defaults live here.

| Constant | Default | Description |
|----------|---------|-------------|
| `SENSOR_INTERVAL_MS` | 100 | Default poll period (ms). 100 = 10 Hz. Adjustable at runtime from the dashboard. |
| `FILTER_MAX_WINDOW` | 21 | Hard maximum for filter window (sets array size) |
| `FILTER_DEFAULT_WINDOW` | 5 | Window size on startup. Adjustable at runtime. |
| `AP_SSID` | `"ICP-Monitor"` | WiFi network name |
| `AP_PASSWORD` | `"pressure"` | WiFi password |
| `BAT_VOLT_FULL` | 4.2 V | LiPo full voltage |
| `BAT_VOLT_EMPTY` | 3.0 V | LiPo empty voltage |
| `BATTERY_INTERVAL_MS` | 5000 | Battery read period (ms) |

---

### `sensor.h/.cpp`
Encapsulates all communication with the Honeywell ABP2.

**Protocol**: I2C at address `0x28`. Each measurement cycle:
1. Write 3-byte command `[0xAA, 0x00, 0x00]` to trigger conversion.
2. Wait 10 ms for the sensor ASIC.
3. Read 7 bytes: `[status, P[23:16], P[15:8], P[7:0], T[23:16], T[15:8], T[7:0]]`.
4. Validate status byte: `0x40` = powered on and not busy.

**Transfer function** (Honeywell ABP2 datasheet, 10%–90% calibration):
```
pressure_psi = ((counts − OUT_MIN) × (P_MAX − P_MIN)) / (OUT_MAX − OUT_MIN) + P_MIN

  OUT_MIN = 1,677,722   (10% × 2^24)
  OUT_MAX = 15,099,494  (90% × 2^24)
  P_MIN   = 0.0 psi,  P_MAX = 1.0 psi
```

**Temperature**:
```
temp_c = (counts × 200) / 16,777,215 − 50
```

**`SensorResult` enum**:

| Value | Meaning |
|-------|---------|
| `SENSOR_OK` | Valid reading |
| `SENSOR_ERROR` | I2C failure or bad status byte |
| `SENSOR_OVERRANGE` | Counts ≥ OUT_MAX (above 1 PSI / 51.7 mmHg) |
| `SENSOR_UNDERRANGE` | Counts ≤ OUT_MIN (below 0 PSI) |

**Unit conversion**: `1 PSI = 51.7149 mmHg`

---

### `filter.h/.cpp`
A median filter applied to every valid pressure reading after the zero offset is subtracted.

**How it works**: A circular buffer of the last N samples is maintained. On each new sample, a temporary sorted copy is produced via insertion sort (≤ N² / 2 comparisons — negligible on ESP32 for N ≤ 21), and the middle element is returned.

**Why median**:
- A single erroneous sample (I2C glitch, electrical transient) can only become the median if it represents the majority of the buffer — a single bad read is invisible.
- Unlike threshold-based spike rejection, there is no parameter to tune and no risk of suppressing a real rapid pressure change. If pressure genuinely changes, the majority of the buffer follows and the median tracks it immediately.
- Unlike a rolling average, spikes do not bleed proportionally into the output.

**Runtime adjustment**: The window size is adjustable from the Prototyping Panel without reflashing. `filterSetWindow(n)` enforces odd values (required for a unique median) and resets the buffer. The maximum window is `FILTER_MAX_WINDOW` (21 samples).

The filter is also reset on zero/tare so the new baseline is established from fresh samples only.

---

### `battery.h/.cpp`
Reads battery voltage via the ADC on the XIAO ESP32-C5.

The board has a 1:2 resistor voltage divider (two 100 kΩ) connecting battery+ to the ADC input. 16 samples are averaged using `analogReadMilliVolts()`, then multiplied by 2 to recover actual voltage.

- Battery % is a linear interpolation: 3.0 V = 0%, 4.2 V = 100%.
- If voltage < 2.5 V (pin floating, no LiPo connected), `batteryPercent()` returns `−1` and the dashboard shows **USB ⚡** instead of a percentage.
- Charging is inferred when voltage > 4.15 V.
- Battery is sampled every 5 seconds to avoid ADC contention with the main loop.

---

### `webserver.h/.cpp`
Manages the WiFi access point, HTTP server, WebSocket, and captive portal.

**WiFi AP**: The ESP32 creates a standalone network (`WIFI_AP` mode) at `192.168.4.1`. No external router or internet required.

**Captive portal**: Platform-specific detection endpoints prevent phones from dropping the network as "no internet":

| Endpoint | Platform |
|----------|----------|
| `/hotspot-detect.html` | iOS / macOS |
| `/generate_204` | Android |
| `/connecttest.txt` | Windows |

A `DNSServer` resolves all DNS queries to the ESP32's IP, completing captive portal behaviour on all platforms. All other paths redirect to `http://192.168.4.1/`.

**WebSocket** at `/ws`:

Incoming commands (client → server):

| Command | Payload | Action |
|---------|---------|--------|
| `zero` | — | Stores current raw reading as zero offset; resets filter |
| `setRate` | `"value": <Hz>` | Changes sample rate (0.1–50 Hz); takes effect immediately |
| `setFilter` | `"value": <N>` | Changes median window (1–21 samples); resets filter buffer |

Outgoing data frame (server → client, broadcast at the current sample rate):

```json
{
  "p":    25.3,   // Filtered pressure in mmHg (1 decimal)
  "t":    36.8,   // Sensor temperature in °C
  "bat":  78,     // Battery % (0–100), or -1 = USB only
  "chg":  false,  // Charging indicator
  "ts":   123456, // millis() timestamp
  "ok":   true,   // Sensor read succeeded
  "ovr":  false,  // Sensor overrange
  "rate": 100,    // Current sample rate in tenths-of-Hz (100 = 10 Hz, 1 = 0.1 Hz)
  "fwin": 5       // Current filter window in samples
}
```

`rate` is transmitted as **tenths-of-Hz** (integer) to represent sub-1 Hz rates such as 0.1 Hz without floating point in JSON.

---

### `dashboard.h`
The complete web UI — HTML, CSS, and JavaScript — stored as a single `PROGMEM` raw string literal and served directly from flash. This avoids LittleFS, partition table changes, and a separate file upload step.

**Main display**:
- Large pressure reading (mmHg), color-coded green (normal) / red (alert) / orange (overrange)
- Status badge: NORMAL / ALERT — HIGH PRESSURE / SENSOR OVERRANGE / SENSOR ERROR
- Sensor temperature and connection status

**Chart**: Custom scrolling line chart built on the Canvas 2D API. Maintains a 600-point circular buffer (60 seconds at 10 Hz). Renders grid, dashed red threshold line, pressure trace, and fill. No external libraries — CDN resources are unavailable on the isolated AP network.

**Alert system**: When pressure ≥ threshold, the screen flashes red and an 880 Hz sine wave beep is generated via the Web Audio API, rate-limited to once every 2 seconds. A **Start Monitoring** button on first load initializes the `AudioContext` to satisfy iOS Safari's user-gesture audio requirement.

**Threshold**: Configurable via +/− stepper or direct input. Default 30 mmHg (standard fasciotomy criterion). Persisted in `localStorage`.

**Zero button**: Sends `{"cmd":"zero"}` to the ESP32. The firmware stores the current raw reading as the pressure offset; the filter resets. Confirmation text appears briefly on the chart.

**Prototyping Panel** (collapsible, labelled DEV):

- *Sample Rate*: Preset buttons — 0.1 / 1 / 5 / 10 / 20 Hz. Sends `setRate` command; active button updates to reflect actual ESP32 rate on next data frame.
- *Median Window*: Stepper in steps of 2 (1–21, always odd). Displays window size in both samples and milliseconds. Sends `setFilter` command.
- *Battery Life Estimator*: Enter battery capacity in mAh. Computes estimated current draw and battery life using the model:
  ```
  draw = 130 mA (WiFi AP) + 20 mA (CPU) + rate_hz × 0.3 mA (sampling overhead)
  life = capacity_mAh / draw_mA
  ```
  Updates live as sample rate or capacity changes. Note: WiFi dominates; rate has a minor effect on total draw.

Both rate and filter window controls stay in sync with the ESP32 — if the page is refreshed, the incoming `rate` and `fwin` fields in the next data frame restore the UI to the actual current settings.

---

## Getting Started

### Prerequisites

- Arduino IDE 2.x
- Seeed ESP32 board package — add to Additional Board Manager URLs:
  ```
  https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
  ```

### Library Installation

Install via **Sketch → Include Library → Manage Libraries**:

| Library | Author | Notes |
|---------|--------|-------|
| ArduinoJson | Benoit Blanchon | Version 7.x |
| ESPAsyncWebServer | Mathieu Carbou | **Required** — use this fork for ESP32 core 3.x |
| AsyncTCP | Mathieu Carbou | Installed as dependency of ESPAsyncWebServer |

> **Important**: The older `me-no-dev` versions of ESPAsyncWebServer and AsyncTCP are incompatible with ESP32 Arduino core 3.x and will fail to compile with `HTTP_GET not declared`.

### Flashing

1. Connect XIAO ESP32-C5 via USB-C.
2. Open `ICP_Monitor/ICP_Monitor.ino` in Arduino IDE.
3. **Tools → Board → XIAO_ESP32C5**
4. **Tools → Port** → select the correct COM port.
5. Click **Upload**.

Verify with **Tools → Serial Monitor** at 115200 baud (press RESET after opening):

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

1. **Settings → WiFi → ICP-Monitor** (password: `pressure`)
2. Safari opens automatically, or navigate to `http://192.168.4.1`
3. Tap **Start Monitoring** to enable audio alerts
4. Live readings begin immediately

---

## Clinical Use

### Zeroing (Tare)

Before inserting the needle, expose the sensor to atmosphere and tap **ZERO**. The current reading is stored as the offset; all subsequent values are displayed relative to zero. The offset resets on power cycle — re-zero at the start of each procedure.

### Alert Threshold

Default: **30 mmHg** (standard compartment syndrome fasciotomy criterion). Adjust with +/− or direct input. Persists in browser `localStorage` across refreshes.

When pressure ≥ threshold:
- Pressure display turns red with pulsing **ALERT — HIGH PRESSURE** badge
- 880 Hz audio beep every 2 seconds

### Overrange

If the sensor saturates (pressure > 1 PSI / 51.7 mmHg), the display shows **OVR** in orange. The chart pauses until pressure returns within range.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Sensor ERROR on display | Wiring issue or missing pull-ups | Check SDA→D4, SCL→D5; add 4.7 kΩ pull-ups |
| Pressure reads ~−26 mmHg | Not zeroed | Tap ZERO before measurement |
| Battery shows "USB" | No LiPo connected | Connect 3.7V LiPo to B+/B− pads |
| Battery shows 0% | Wrong ADC pin / no divider | Check raw mV in Serial Monitor at startup |
| Dashboard won't load | Phone dropped AP WiFi | Reconnect to "ICP-Monitor" in WiFi settings |
| No alert beep | iOS audio locked | Tap "Start Monitoring" first |
| `HTTP_GET` compile error | Wrong ESPAsyncWebServer fork | Remove old library; install Mathieu Carbou fork |

---

## Configuration Reference

`ICP_Monitor/config.h` — defaults only; sample rate and filter window are overridable at runtime from the Prototyping Panel.

```cpp
// I2C
#define SDA_PIN     D4
#define SCL_PIN     D5
#define I2C_ADDR    0x28

// Sensor transfer function (do not modify — matches ABP2 datasheet)
const float P_MIN        = 0.0;
const float P_MAX        = 1.0;
const float OUT_MIN      = 1677722.0;
const float OUT_MAX      = 15099494.0;
const float PSI_TO_MMHG  = 51.7149;

// Filter
#define FILTER_MAX_WINDOW     21    // Hard array size limit (must be odd)
#define FILTER_DEFAULT_WINDOW  5    // Window on startup

// Timing
#define SENSOR_INTERVAL_MS   100   // Default 10 Hz (runtime adjustable)
#define BATTERY_INTERVAL_MS  5000  // 0.2 Hz (fixed)

// WiFi
#define AP_SSID     "ICP-Monitor"
#define AP_PASSWORD "pressure"

// Battery thresholds (3.7V LiPo)
const float BAT_VOLT_FULL     = 4.2;
const float BAT_VOLT_EMPTY    = 3.0;
const float BAT_VOLT_CHARGING = 4.15;
```
