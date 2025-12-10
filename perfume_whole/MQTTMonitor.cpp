#include "MQTTMonitor.h"
#include "NetworkManager.h"
#include "SystemConfig.h"
#include <ArduinoJson.h>

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


  // mqttHandler.setInitialMessage(("PerfumeDispenser/" + String(deviceESN) + "/status").c_str(), "Online");
  //mqttHandler.setInitialMessage("PerfumeDispenser/DeviceStatus", "Online");

  // === Subscriptions ===
  // mqttHandler.addSubscriptionTopic(("PerfumeDispenser/" + String(deviceESN) + "/#").c_str());
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/RequestSettings");
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/Settings");
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/ControlFlag/1");
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/ControlFlag/2");
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/ControlFlag/3");
  mqttHandler.addSubscriptionTopic("PerfumeDispenser/ControlFlag/4");
  mqttHandler.connect();

  unsigned long lastWatchdogUpdate = 0;
  const unsigned long watchdogInterval = 180000;  // 3 minutes

  // ✅ Confirm Wi-Fi and MQTT connectivity before sending retained status
  if (WiFi.status() == WL_CONNECTED) {
    mqttHandler.checkConnectivity();
    mqttHandler.startup("PerfumeDispenser/DeviceStatus", "Online", true);
  }


  // ✅ Request settings immediately on startup/reset
  if (WiFi.status() == WL_CONNECTED) {
    String requestTopic = "PerfumeDispenser/RequestSettings";
    String requestPayload = "request settings";
    mqttHandler.publish(requestTopic.c_str(), requestPayload.c_str());
    Serial.printf("[MQTTMonitor] Startup → Requested settings → %s : %s\n",
                  requestTopic.c_str(), requestPayload.c_str());
  }



  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      mqttHandler.checkConnectivity();

      // === Message Handling ===
      if (mqttHandler.messageAvailable()) {
        String topic = mqttHandler.getMessageTopic();
        String payload = mqttHandler.getMessagePayload();

        Serial.printf("[MQTTMonitor] Received → %s : %s\n", topic.c_str(), payload.c_str());

        // ✅ Fixed variable names
        if (topic.equals("PerfumeDispenser/Settings")) {
          handleSettingsMessage(payload);
        } else {
          handleIncomingMQTTMessage(topic, payload);
        }

        mqttHandler.clearMessageFlag();
      }

      // === Watchdog heartbeat ===
      unsigned long currentMillis = millis();
      if (currentMillis - lastWatchdogUpdate >= watchdogInterval) {
        publishWatchdogHeartbeat();
        lastWatchdogUpdate = currentMillis;
      }
    }

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

  // ✅ Updated formula:
  // dispenses = ((totalPesos / relayPrice) * relayDuration) / 1000
  float dispenses = 0.0f;
  if (relayPrice > 0) {
    dispenses = ((float)totalPesos / (float)relayPrice) * ((float)relayDuration / 1000.0f);
  }

  String topic = "PerfumeDispenser/Transaction";
  String payload = "{";
  payload += "\"id\":" + String(relayNum) + ",";
  payload += "\"price\":" + String(totalPesos) + ",";
  payload += "\"dispenses\":" + String(dispenses, 2);
  payload += "}";

  mqttHandler.publish(topic.c_str(), payload.c_str());
  Serial.printf("[MQTTMonitor] Published Transaction → %s : %s\n", topic.c_str(), payload.c_str());
}


// === Handle Device Configuration Messages ===
void handleIncomingMQTTMessage(const String &topic, const String &payload) {
  // Example topic: PerfumeDispenser/ControlFlag/1
  String base = "PerfumeDispenser/ControlFlag/";
  if (!topic.startsWith(base)) return;

  int relayNum = topic.substring(base.length()).toInt();  // Extract the relay number
  if (relayNum < 1 || relayNum > 4) return;               // Only relays 1–4

  // Payload is expected to be "Enable" or "Disable"
  String cmd = payload;
  cmd.toLowerCase();  // normalize

  if (cmd == "enable") {
    dispenseStatus[relayNum - 1] = false;
    saveDispenseStatusToEEPROM(relayNum, false);
    Serial.printf("[MQTTMonitor] Relay %d → ENABLED\n", relayNum);
  } else if (cmd == "disable") {
    dispenseStatus[relayNum - 1] = true;
    saveDispenseStatusToEEPROM(relayNum, true);
    Serial.printf("[MQTTMonitor] Relay %d → DISABLED\n", relayNum);
  } else {
    Serial.printf("[MQTTMonitor] Unknown command for Relay %d → %s\n", relayNum, payload.c_str());
  }
}


// === ✅ Handle PerfumeDispenser/Settings ===
void handleSettingsMessage(const String &payload) {
  StaticJsonDocument<2048> doc;  // safer for large JSON payloads

  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.printf("[MQTTMonitor] JSON Parse Error: %s\n", error.c_str());
    return;
  }

  if (!doc.is<JsonArray>()) {
    Serial.println("[MQTTMonitor] Invalid settings format (not array)");
    return;
  }

  JsonArray arr = doc.as<JsonArray>();

  for (JsonObject item : arr) {
    int id = item["id"] | 0;
    unsigned long duration = item["duration"] | 0;
    unsigned long price = (unsigned long)item["price"].as<float>();  // ✅ safely converts 5.0 → 5

    if (id < 1 || id > 4) continue;

    relayDurations[id - 1] = duration;
    relayPrices[id - 1] = price;

    saveRelayDurationToEEPROM(id, duration);
    saveRelayPriceToEEPROM(id, price);

    Serial.printf("[MQTTMonitor] Relay %d updated → Duration: %lu ms | Price: %lu\n",
                  id, duration, price);
  }
}

// === Watchdog Timer Publish Function ===
void publishWatchdogHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) return;

  // 1️⃣ Pulse the hardware watchdog
  digitalWrite(WDT_PIN, HIGH);
  delay(10);
  digitalWrite(WDT_PIN, LOW);

  
  mqttHandler.checkConnectivity();
  mqttHandler.startup("PerfumeDispenser/DeviceStatus", "Online", true);
  // 2️⃣ Publish alive status
  // String watchdogTopic = "PerfumeDispenser/" + String(deviceESN) + "/watchdog";
  // String watchdogPayload = "{\"status\":\"alive\"}";
  //  mqttHandler.publish(watchdogTopic.c_str(), watchdogPayload.c_str());
  // Serial.printf("[MQTTMonitor] Watchdog heartbeat → %s : %s\n", watchdogTopic.c_str(), watchdogPayload.c_str());

  // 3️⃣ Publish request for settings
  //  String requestTopic = "PerfumeDispenser/RequestSettings";
  // String requestPayload = "request settings";
  // mqttHandler.publish(requestTopic.c_str(), requestPayload.c_str());
  // Serial.printf("[MQTTMonitor] Requested settings → %s : %s\n", requestTopic.c_str(), requestPayload.c_str());
}
