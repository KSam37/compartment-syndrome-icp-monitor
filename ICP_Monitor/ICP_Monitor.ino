#include "config.h"
#include "sensor.h"
#include "battery.h"
#include "filter.h"
#include "webserver.h"

// ── Runtime-adjustable settings ──
volatile uint16_t sensorIntervalMs = SENSOR_INTERVAL_MS;  // adjusted via dashboard

// ── Zero (tare) offset ──
float zeroOffsetMmhg = 0.0;
volatile bool zeroRequested = false;

// ── Last valid readings ──
float lastPressureMmhg = 0.0;
float lastTempC = 0.0;
bool  lastSensorOk = false;
bool  lastOverrange = false;

// ── Battery state ──
int  batPercent = -1;
bool batCharging = false;

// ── Timing ──
unsigned long lastSensorRead = 0;
unsigned long lastBatteryRead = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ICP Monitor ===");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // HIGH = off (active low on XIAO)

  sensorInit();
  filterInit();
  Serial.println("Sensor initialized (I2C)");

  batteryInit();
  Serial.println("Battery monitor initialized");

  setupWiFiAP();

  setZeroCallback([]() {
    zeroRequested = true;
  });

  setRateCallback([](uint16_t intervalMs) {
    sensorIntervalMs = intervalMs;
  });

  setFilterCallback([](uint8_t window) {
    filterSetWindow(window);  // filterSetWindow resets the buffer internally
  });

  setupWebServer();

  Serial.println("Ready. Connect to WiFi: " AP_SSID);
  Serial.println("Open browser to: http://192.168.4.1");
}

void loop() {
  processDNS();

  unsigned long now = millis();

  // ── Read sensor at runtime-adjustable rate ──
  if (now - lastSensorRead >= sensorIntervalMs) {
    lastSensorRead = now;

    float psi = 0, tempC = 0;
    SensorResult result = sensorRead(psi, tempC);

    lastOverrange = (result == SENSOR_OVERRANGE);
    lastSensorOk  = (result == SENSOR_OK);

    if (result == SENSOR_OK) {
      float rawMmhg = psiToMmhg(psi);

      if (zeroRequested) {
        zeroOffsetMmhg = rawMmhg;
        zeroRequested = false;
        filterInit();
        Serial.printf("Zeroed. Offset = %.1f mmHg\n", zeroOffsetMmhg);
      }

      float offsetMmhg = rawMmhg - zeroOffsetMmhg;
      lastPressureMmhg = filterSample(offsetMmhg);
      lastTempC = tempC;
    }

    // Compute Hz from current interval for the broadcast payload (float * 100 to preserve 0.1)
    // Sent as integer tenths-of-Hz so JSON stays simple: 1 = 0.1 Hz, 10 = 1 Hz, 100 = 10 Hz
    uint16_t rateTenths = (uint16_t)round(10000.0f / sensorIntervalMs);

    broadcastSensorData(lastPressureMmhg, lastTempC,
                        batPercent, batCharging,
                        lastSensorOk, lastOverrange,
                        rateTenths, filterGetWindow());

    // Blink LED on each measurement (active low on XIAO)
    digitalWrite(LED_BUILTIN, LOW);
    delay(20);
    digitalWrite(LED_BUILTIN, HIGH);

    // Serial debug — print every ~1 s regardless of sample rate
    static uint32_t lastDbgMs = 0;
    if (now - lastDbgMs >= 1000) {
      lastDbgMs = now;
      Serial.printf("P: %.1f mmHg | T: %.1f C | Bat: %d%% %s | %s | %.1f Hz | fwin=%d\n",
        lastPressureMmhg, lastTempC, batPercent,
        batCharging ? "(chg)" : "",
        lastOverrange ? "OVERRANGE" : (lastSensorOk ? "OK" : "ERROR"),
        rateTenths / 10.0f, filterGetWindow());
    }
  }

  // ── Read battery every 5 seconds ──
  if (now - lastBatteryRead >= BATTERY_INTERVAL_MS) {
    lastBatteryRead = now;
    float voltage = readBatteryVoltage();
    batPercent = batteryPercent(voltage);
    batCharging = isCharging(voltage);
  }

  cleanupClients();
}
