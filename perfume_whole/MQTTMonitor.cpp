#include "MQTTMonitor.h"
#include "NetworkManager.h"
#include "SystemConfig.h"
#include <ArduinoJson.h>
#include "RelayHandler.h"

TaskHandle_t mqttMonitorTaskHandle;

void handleIncomingMQTTMessage(const String &topic, const String &payload);
void handleSettingsMessage(const String &payload);
void publishWatchdogHeartbeat();

// === MQTT Monitor Task ===
void MQTTMonitor_Routine(void *pvParameters) {
  WiFiCreds_t wifiCreds = loadWiFiCredsFromEEPROM();
  loadMQTTConfigFromEEPROM();
  loadDeviceESNFromEEPROM();

  Serial.println("[MQTTMonitor] Initializing MQTT...");

  pinMode(WDT_PIN, OUTPUT);
  digitalWrite(WDT_PIN, LOW);

  String willTopic = "PerfumeDispenser/LastWill";
  String willMessage = "{\"client_id\":\"" + String(deviceESN) + "\", \"status\":\"offline\"}";

  mqttHandler.init(
    mqttConfig.mqttServer,
    mqttConfig.mqttPort,
    mqttConfig.mqttUser,
    mqttConfig.mqttPassword,
    deviceESN,
    willTopic.c_str(),
    willMessage.c_str());

  // === Subscriptions ===
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/RequestSettings");
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/Settings");
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/ControlFlag/1");
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/ControlFlag/2");
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/ControlFlag/3");
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/ControlFlag/4");
  mqttHandler.connect();

  unsigned long lastWatchdogUpdate = 0;
  const unsigned long watchdogInterval = 180000;  // 3 minutes

  // === Confirm connectivity before retained status ===
  if (WiFi.status() == WL_CONNECTED) {
    mqttHandler.checkConnectivity();
    mqttHandler.startup("PerfumeDispenser/DeviceStatus", "Online", true);
  }

  // === Request settings on startup ===
  if (WiFi.status() == WL_CONNECTED) {
    mqttHandler.publish("PerfumeDispenser/RequestSettings", "request settings");
    Serial.println("[MQTTMonitor] Startup → Requested settings");
  }

  // === EDGE STATE TRACKERS ===
  static bool wifiWasOK = true;
  static bool mqttWasOK = true;

  for (;;) {

    bool wifiOK = (WiFi.status() == WL_CONNECTED);

    // =========================================
    // WIFI EDGE DETECTION: CONNECTED → LOST
    // =========================================
    if (!wifiOK && wifiWasOK) {

      for (int i = 0; i < 4; i++) {
        dispenseStatus[i] = true;  // disable all relays
      }

      relayHandler.update();
      Serial.println("[MQTTMonitor] WIFI LOST → Relays DISABLED");
    }

    if (wifiOK) {

      bool mqttOK = mqttHandler.checkConnectivity();

      // =========================================
      // MQTT EDGE DETECTION: CONNECTED → LOST
      // =========================================
      if (!mqttOK && mqttWasOK) {

        for (int i = 0; i < 4; i++) {
          dispenseStatus[i] = true;  // disable all relays
        }

        relayHandler.update();
        Serial.println("[MQTTMonitor] MQTT LOST → Relays DISABLED");
      }

      // =========================================
      // NORMAL MQTT MESSAGE HANDLING
      // =========================================
      if (mqttOK && mqttHandler.messageAvailable()) {

        for (int i = 0; i < 4; i++) {
          dispenseStatus[i] = loadDispenseStatusFromEEPROM(i + 1);
          relayDurations[i] = loadRelayDurationFromEEPROM(i + 1);
        }

        relayHandler.update();

        String topic = mqttHandler.getMessageTopic();
        String payload = mqttHandler.getMessagePayload();

        Serial.printf("[MQTTMonitor] Received → %s : %s\n",
                      topic.c_str(), payload.c_str());

        if (topic.equals("PerfumeDispenser/Settings")) {
          handleSettingsMessage(payload);
        } else {
          handleIncomingMQTTMessage(topic, payload);
        }

        mqttHandler.clearMessageFlag();
      }

      // =========================================
      // WATCHDOG HEARTBEAT
      // =========================================
      unsigned long currentMillis = millis();
      if (currentMillis - lastWatchdogUpdate >= watchdogInterval) {
        publishWatchdogHeartbeat();
        lastWatchdogUpdate = currentMillis;
      }

      mqttWasOK = mqttOK;
    }

    wifiWasOK = wifiOK;
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// === Start Task ===
void startMQTTMonitorTask() {
  xTaskCreatePinnedToCore(
    MQTTMonitor_Routine,
    "MQTTMonitor",
    6144,
    NULL,
    1,
    &mqttMonitorTaskHandle,
    1);
  Serial.println("[MQTTMonitor] Task started.");
}

// === Publish Perfume Transaction ===
void publishRelayEventMQTT(int relayNum, int totalPesos, const char *state) {
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long relayPrice = relayPrices[relayNum - 1];
  unsigned long relayDuration = relayDurations[relayNum - 1];

  float dispenses = 0.0f;
  if (relayPrice > 0) {
    dispenses = ((float)totalPesos / (float)relayPrice) * ((float)relayDuration / 1000.0f);
  }

  String payload = "{";
  payload += "\"id\":" + String(relayNum) + ",";
  payload += "\"price\":" + String(totalPesos) + ",";
  payload += "\"dispenses\":" + String(dispenses, 2);
  payload += "}";

  mqttHandler.publish("PerfumeDispenser/Transaction", payload.c_str());
}

// === Handle Control Flags ===
void handleIncomingMQTTMessage(const String &topic, const String &payload) {
  String base = "PerfumeDispenser/ControlFlag/";
  if (!topic.startsWith(base)) return;

  int relayNum = topic.substring(base.length()).toInt();
  if (relayNum < 1 || relayNum > 4) return;

  String cmd = payload;
  cmd.toLowerCase();

  if (cmd == "enable") {
    dispenseStatus[relayNum - 1] = false;
    saveDispenseStatusToEEPROM(relayNum, false);
  } else if (cmd == "disable") {
    dispenseStatus[relayNum - 1] = true;
    saveDispenseStatusToEEPROM(relayNum, true);
  }

  relayHandler.update();
}

// === Handle Settings ===
void handleSettingsMessage(const String &payload) {
  StaticJsonDocument<2048> doc;

  if (deserializeJson(doc, payload)) return;
  if (!doc.is<JsonArray>()) return;

  for (JsonObject item : doc.as<JsonArray>()) {

    int id = item["id"] | 0;
    unsigned long duration = item["duration"] | 0;
    unsigned long price = (unsigned long)item["price"].as<float>();

    if (id < 1 || id > 4) continue;

    relayDurations[id - 1] = duration;
    relayPrices[id - 1] = price;

    saveRelayDurationToEEPROM(id, duration);
    saveRelayPriceToEEPROM(id, price);
  }
}

// === Watchdog Heartbeat ===
void publishWatchdogHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) return;

  digitalWrite(WDT_PIN, HIGH);
  delay(10);
  digitalWrite(WDT_PIN, LOW);

  mqttHandler.checkConnectivity();
  mqttHandler.startup("PerfumeDispenser/DeviceStatus", "Online", true);
}
