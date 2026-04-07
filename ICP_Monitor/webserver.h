#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>

void setupWiFiAP();
void setupWebServer();

// Broadcast a sensor data frame to all connected WebSocket clients.
// rate_hz and fwin are echoed back so the dashboard stays in sync after refresh.
// rate_tenths: sample rate in units of 0.1 Hz (e.g. 1 = 0.1 Hz, 10 = 1 Hz, 100 = 10 Hz)
void broadcastSensorData(float pressure_mmhg, float temp_c,
                         int bat_pct, bool charging,
                         bool sensor_ok, bool overrange,
                         uint16_t rate_tenths, uint8_t fwin);

// Callbacks — register before calling setupWebServer()
void setZeroCallback  (void (*cb)());
void setRateCallback  (void (*cb)(uint16_t intervalMs));
void setFilterCallback(void (*cb)(uint8_t window));

void cleanupClients();
void processDNS();

#endif
