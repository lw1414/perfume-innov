#include <Arduino.h>
#include <WiFi.h>
#include "SystemConfig.h"
#include "ShiftRegister.h"
#include "CLIHandler.h"
#include "NetworkManager.h"
#include "MQTTHandler.h"
#include "MQTTMonitor.h"
#include "CoinHandler.h"
#include "RelayHandler.h"  // <-- new relay library

// === Global MQTT Handler ===
MQTTHandler mqttHandler;

// === Shift Register (for relays) ===
//ShiftRegister OUTPUT_CONTROL_PORT(OUT_CTRL_DIN, OUT_CTRL_CS, OUT_CTRL_CLK, 1);

// === Button debounce ===
unsigned long lastButtonPress[4] = { 0, 0, 0, 0 };
const unsigned long debounceDelay = 300;  // ms

// === Relay Handler ===
//RelayHandler relayHandler(OUTPUT_CONTROL_PORT);

void setup() {
  Serial.begin(115200);
  delay(100);

  // === Initialize System ===
  initSystemConfig();
  CLIHandler::init();

  // === Start Relay Handler ===
  relayHandler.begin();


  // == Initial State of led lights == //
  for (int i = 0; i < 4; i++) {
    dispenseStatus[i] = true;  // system online → bits 5-8 OFF
  }
  relayHandler.update();  // reflect change on shift register

  // === Pin setup ===
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT);
  pinMode(BUTTON4_PIN, INPUT);

  Serial.println("========================================");
  Serial.println("   Coin + 4-Button Relay System Started ");
  Serial.println("========================================");

  // === Print system summary ===
  printSystemSummary();

  // === Start Network Monitor Task ===
  startNetworkMonitorTask();

  // === Wait for WiFi connection ===
  Serial.print("[WiFi] Waiting for connection");
  unsigned long wifiStart = millis();
  const unsigned long WIFI_WAIT_MS = 20000;  // 20s
  while (!networkInfo.wifiConnected && (millis() - wifiStart) < WIFI_WAIT_MS) {
    Serial.print(".");
    delay(500);
  }
  if (networkInfo.wifiConnected) {
    Serial.println("\n[WiFi] Connected.");
  } else {
    Serial.println("\n[WiFi] Not connected (timed out). MQTT will be attempted later.");
  }

  // === Start MQTT Monitor Task ===
  startMQTTMonitorTask();

  // === Start Coin Task ===
  startCoinTask();

  Serial.println("System ready. Use AT+TOTAL? or AT? for commands.\n");
}

void loop() {
  CLIHandler::handleSerial();
  relayHandler.update();  // handle relay timing AND update shift bits 5-8

  unsigned long now = millis();
  int totalPesos = getTotalPesos();

  // === Button inputs ===
  if (digitalRead(BUTTON1_PIN) == LOW && now - lastButtonPress[0] > debounceDelay) {
    lastButtonPress[0] = now;
    if (!dispenseStatus[3] && totalPesos > 0) {
      relayHandler.activateRelayAsync(4, relayDurations[3]);
    }
  }
  if (digitalRead(BUTTON2_PIN) == LOW && now - lastButtonPress[1] > debounceDelay) {
    lastButtonPress[1] = now;
    if (!dispenseStatus[2] && totalPesos > 0) {
      relayHandler.activateRelayAsync(3, relayDurations[2]);
    }
  }
  if (digitalRead(BUTTON3_PIN) == LOW && now - lastButtonPress[2] > debounceDelay) {
    lastButtonPress[2] = now;
    if (!dispenseStatus[1] && totalPesos > 0) {
      relayHandler.activateRelayAsync(2, relayDurations[1]);
    }
  }
  if (digitalRead(BUTTON4_PIN) == LOW && now - lastButtonPress[3] > debounceDelay) {
    lastButtonPress[3] = now;
    if (!dispenseStatus[0] && totalPesos > 0) {
      relayHandler.activateRelayAsync(1, relayDurations[0]);
    }
  }

  vTaskDelay(10 / portTICK_PERIOD_MS);
}

// === System Summary ===
void printSystemSummary() {
  WiFiCreds_t wifiCreds = loadWiFiCredsFromEEPROM();
  loadMQTTConfigFromEEPROM();
  loadDeviceESNFromEEPROM();

  Serial.println("\n--- System Configuration Summary ---");
  Serial.printf("Device ESN: %s\n", deviceESN);
  Serial.printf("WiFi SSID: %s\n", wifiCreds.ssid);
  Serial.printf("MQTT Server: %s\n", mqttConfig.mqttServer);
  Serial.printf("MQTT Port: %d\n", mqttConfig.mqttPort);
  Serial.printf("MQTT User: %s\n", mqttConfig.mqttUser);
  Serial.printf("MQTT Pass: %s\n", mqttConfig.mqttPassword);

  for (int i = 0; i < 4; i++) {
    Serial.printf(
      "Relay %d → Duration: %lu ms | Dispense=%d | Price=₱%lu\n",
      i + 1,
      relayDurations[i],
      dispenseStatus[i],
      relayPrices[i]  // ✅ Added price display
    );
  }

  Serial.println("------------------------------------\n");
}
