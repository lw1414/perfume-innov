#include "NetworkManager.h"
#include "SystemConfig.h"

TaskHandle_t xTaskHandle_NetworkMonitor = NULL;
NetworkInfo_t networkInfo = { false, "", "", 0, false };

void startNetworkMonitorTask() {
  xTaskCreatePinnedToCore(
    NetworkMonitorTask,
    "Network Monitor",
    4096,
    NULL,
    1,
    &xTaskHandle_NetworkMonitor,
    1
  );
}

void NetworkMonitorTask(void* pvParameters) {
  Serial.print("[NetworkManager] Running on core ");
  Serial.println(xPortGetCoreID());

  vTaskDelay(1000);

  WiFiManager wm;

  // -------------------------------
  // WiFiManager configuration
  // -------------------------------
  wm.setConfigPortalTimeout(240);   // 4 minutes
  wm.setConnectTimeout(15);
  wm.setWiFiAutoReconnect(true);

  // -------------------------------
  // Load WiFi creds from EEPROM
  // -------------------------------
  WiFiCreds_t creds = loadWiFiCredsFromEEPROM();
  if (strlen(creds.ssid) > 0) {
    wm.preloadWiFi(creds.ssid, creds.password);
    Serial.println("[NetworkManager] EEPROM WiFi credentials preloaded");
  }

  // -------------------------------
  // Start WiFiManager
  // -------------------------------
  bool res = wm.autoConnect(deviceESN, "innovation");

  if (!res) {
    Serial.println("[NetworkManager] WiFi connect failed → rebooting");
    ESP.restart();
  }

  // -------------------------------
  // Connected successfully
  // -------------------------------
  Serial.print("[NetworkManager] Connected to: ");
  Serial.println(WiFi.SSID());

  networkInfo.wifiConnected = true;
  networkInfo.SSID = WiFi.SSID();
  networkInfo.password = WiFi.psk();
  networkInfo.RSSI = WiFi.RSSI();

  // -------------------------------
  // SAVE CREDENTIALS TO EEPROM
  // -------------------------------
  WiFiCreds_t newCreds;
  memset(&newCreds, 0, sizeof(newCreds));

  strncpy(newCreds.ssid, WiFi.SSID().c_str(), sizeof(newCreds.ssid) - 1);
  strncpy(newCreds.password, WiFi.psk().c_str(), sizeof(newCreds.password) - 1);

  saveWiFiCredsToEEPROM(newCreds);
  Serial.println("[NetworkManager] WiFi credentials saved to EEPROM");

  // -------------------------------
  // Monitor WiFi connection
  // -------------------------------
  for (;;) {
    networkInfo.wifiConnected = (WiFi.status() == WL_CONNECTED);
    networkInfo.RSSI = WiFi.RSSI();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
