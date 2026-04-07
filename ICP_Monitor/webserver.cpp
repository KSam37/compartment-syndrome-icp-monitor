#include "webserver.h"
#include "config.h"
#include "dashboard.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_http_server.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static DNSServer dnsServer;

static void (*_zeroCallback)()                  = nullptr;
static void (*_rateCallback)(uint16_t intervalMs) = nullptr;
static void (*_filterCallback)(uint8_t window)    = nullptr;

static const byte DNS_PORT = 53;
static IPAddress  apIP(192, 168, 4, 1);

void setZeroCallback  (void (*cb)())                   { _zeroCallback   = cb; }
void setRateCallback  (void (*cb)(uint16_t intervalMs)) { _rateCallback   = cb; }
void setFilterCallback(void (*cb)(uint8_t window))      { _filterCallback = cb; }

void setupWiFiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(100);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.print("AP started. IP: ");
  Serial.println(WiFi.softAPIP());
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", DASHBOARD_HTML);
  });

  // Captive portal
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
  });
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Microsoft Connect Test");
  });
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });

  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      Serial.printf("WS client #%u connected\n", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.printf("WS client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = '\0';
        StaticJsonDocument<96> doc;
        if (!deserializeJson(doc, (char*)data)) {
          const char* cmd = doc["cmd"];
          if (!cmd) return;

          if (strcmp(cmd, "zero") == 0 && _zeroCallback) {
            _zeroCallback();
            Serial.println("Zero command received");

          } else if (strcmp(cmd, "setRate") == 0 && _rateCallback) {
            float hz = constrain((float)doc["value"], 0.05f, 50.0f);
            uint16_t intervalMs = (uint16_t)(1000.0f / hz);
            _rateCallback(intervalMs);
            Serial.printf("Rate set to %.2f Hz (%d ms)\n", hz, intervalMs);

          } else if (strcmp(cmd, "setFilter") == 0 && _filterCallback) {
            uint8_t win = constrain((int)doc["value"], 1, FILTER_MAX_WINDOW);
            _filterCallback(win);
            Serial.printf("Filter window set to %d samples\n", win);
          }
        }
      }
    }
  });
  server.addHandler(&ws);

  server.begin();
  Serial.println("Web server started on port 80");
}

void broadcastSensorData(float pressure_mmhg, float temp_c,
                         int bat_pct, bool charging,
                         bool sensor_ok, bool overrange,
                         uint16_t rate_tenths, uint8_t fwin) {
  if (ws.count() == 0) return;

  StaticJsonDocument<192> doc;
  doc["p"]    = round(pressure_mmhg * 10.0) / 10.0;
  doc["t"]    = round(temp_c * 10.0) / 10.0;
  doc["bat"]  = bat_pct;
  doc["chg"]  = charging;
  doc["ts"]   = millis();
  doc["ok"]   = sensor_ok;
  doc["ovr"]  = overrange;
  doc["rate"] = rate_tenths; // tenths-of-Hz: 1=0.1Hz, 10=1Hz, 100=10Hz
  doc["fwin"] = fwin;

  char json[192];
  serializeJson(doc, json);
  ws.textAll(json);
}

void cleanupClients() { ws.cleanupClients(); }
void processDNS()      { dnsServer.processNextRequest(); }
