#include "SystemConfig.h"

// === GLOBAL VARIABLES ===
MQTTConfig_t mqttConfig;
char deviceESN[DEVICE_ESN_MAX_LEN];
unsigned long relayDurations[4] = {0, 0, 0, 0};
uint8_t dispenseStatus[4] = {0, 0, 0, 0};
unsigned long relayPrices[4] = {0, 0, 0, 0};  // ✅ NEW: relay prices

// === MQTT Dynamic Topics ===
String willTopic;
String willMessage;
String initTopic;
String intervalConfig;
String mqttConfiguration;
String topicRelayDuration;
String topicCurrentCredit;

// === Function Definitions ===
void initSystemConfig() {
  EEPROM.begin(EEPROM_SIZE);

  loadMQTTConfigFromEEPROM();
  loadDeviceESNFromEEPROM();

  bool eepromNeedsInit = false;

  // Validate ESN
  if (strlen(deviceESN) == 0 || deviceESN[0] == 0xFF || deviceESN[0] == '\0') {
    strcpy(deviceESN, "ESP32-DEFAULT-ESN");
    eepromNeedsInit = true;
  }

  // Load relay durations, dispense status, and prices
  for (int i = 0; i < 4; i++) {
    relayDurations[i] = loadRelayDurationFromEEPROM(i + 1);
    if (relayDurations[i] == 0xFFFFFFFF || relayDurations[i] == 0)
      relayDurations[i] = 1000; // Default 1 second

    dispenseStatus[i] = loadDispenseStatusFromEEPROM(i + 1);
    if (dispenseStatus[i] == 0xFF)
      dispenseStatus[i] = 0; // Default not dispensing

    relayPrices[i] = loadRelayPriceFromEEPROM(i + 1); // ✅ Load stored price
    if (relayPrices[i] == 0xFFFFFFFF || relayPrices[i] == 0)
      relayPrices[i] = 25; // ✅ Default price = 25 (adjust as needed)
  }

  if (eepromNeedsInit) {
    saveDeviceESNToEEPROM();
    for (int i = 0; i < 4; i++) {
      saveRelayDurationToEEPROM(i + 1, relayDurations[i]);
      saveDispenseStatusToEEPROM(i + 1, dispenseStatus[i]);
      saveRelayPriceToEEPROM(i + 1, relayPrices[i]); // ✅ Store default prices
    }
  }

  initializeDynamicTopics();
  EEPROM.end();
}

// === WiFi ===
void saveWiFiCredsToEEPROM(const WiFiCreds_t& creds) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_WIFI_CRED_ADDR, creds);
  EEPROM.commit();
  EEPROM.end();
}

WiFiCreds_t loadWiFiCredsFromEEPROM() {
  WiFiCreds_t creds;
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_WIFI_CRED_ADDR, creds);
  EEPROM.end();
  return creds;
}

// === MQTT ===
void loadMQTTConfigFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_MQTT_CONFIG_ADDR, mqttConfig);

  // If no valid MQTT data, set defaults
  if (strlen(mqttConfig.mqttServer) == 0 || mqttConfig.mqttServer[0] == 0xFF) {
    strcpy(mqttConfig.mqttServer, "broker.hivemq.com");
    strcpy(mqttConfig.mqttUser, "user");
    strcpy(mqttConfig.mqttPassword, "pass");
    mqttConfig.mqttPort = 1883;
    saveMQTTConfigToEEPROM();
  }

  EEPROM.end();
}

void saveMQTTConfigToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_MQTT_CONFIG_ADDR, mqttConfig);
  EEPROM.commit();
  EEPROM.end();
}

// === ESN ===
void loadDeviceESNFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ESN_ADDR, deviceESN);
  EEPROM.end();

  // Default ESN if empty or invalid
  if (strlen(deviceESN) == 0 || deviceESN[0] == 0xFF) {
    strcpy(deviceESN, "ESP32-DEFAULT-ESN");
    saveDeviceESNToEEPROM();
  }
}

void saveDeviceESNToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_ESN_ADDR, deviceESN);
  EEPROM.commit();
  EEPROM.end();
}

// === Dynamic MQTT Topics ===
void initializeDynamicTopics() {
  willTopic          = String(deviceESN) + "/status/will";
  willMessage        = "Device disconnected";
  initTopic          = String(deviceESN) + "/status/init";
  intervalConfig     = String(deviceESN) + "/config/interval";
  mqttConfiguration  = String(deviceESN) + "/config/mqtt";
  topicRelayDuration = String(deviceESN) + "/relay/duration";
  topicCurrentCredit = String(deviceESN) + "/credit/current";
}

// === Relay Duration ===
void saveRelayDurationToEEPROM(int relayNum, unsigned long duration) {
  int addr;
  switch (relayNum) {
    case 1: addr = EEPROM_RELAY1_DURATION_ADDR; break;
    case 2: addr = EEPROM_RELAY2_DURATION_ADDR; break;
    case 3: addr = EEPROM_RELAY3_DURATION_ADDR; break;
    case 4: addr = EEPROM_RELAY4_DURATION_ADDR; break;
    default: return;
  }
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(addr, duration);
  EEPROM.commit();
  EEPROM.end();
}

unsigned long loadRelayDurationFromEEPROM(int relayNum) {
  int addr;
  switch (relayNum) {
    case 1: addr = EEPROM_RELAY1_DURATION_ADDR; break;
    case 2: addr = EEPROM_RELAY2_DURATION_ADDR; break;
    case 3: addr = EEPROM_RELAY3_DURATION_ADDR; break;
    case 4: addr = EEPROM_RELAY4_DURATION_ADDR; break;
    default: return 0;
  }
  unsigned long val = 0;
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(addr, val);
  EEPROM.end();
  return val;
}

// === Send Interval ===
void saveSendIntervalToEEPROM(uint32_t interval) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_SEND_INTERVAL_ADDR, interval);
  EEPROM.commit();
  EEPROM.end();
}

uint32_t loadSendIntervalFromEEPROM() {
  uint32_t val = 0;
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_SEND_INTERVAL_ADDR, val);
  EEPROM.end();
  return val;
}

// === Dispense Status ===
void saveDispenseStatusToEEPROM(int relayNum, uint8_t status) {
  int addr;
  switch (relayNum) {
    case 1: addr = EEPROM_DISPENSE1_ADDR; break;
    case 2: addr = EEPROM_DISPENSE2_ADDR; break;
    case 3: addr = EEPROM_DISPENSE3_ADDR; break;
    case 4: addr = EEPROM_DISPENSE4_ADDR; break;
    default: return;
  }
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(addr, status);
  EEPROM.commit();
  EEPROM.end();
}

uint8_t loadDispenseStatusFromEEPROM(int relayNum) {
  int addr;
  switch (relayNum) {
    case 1: addr = EEPROM_DISPENSE1_ADDR; break;
    case 2: addr = EEPROM_DISPENSE2_ADDR; break;
    case 3: addr = EEPROM_DISPENSE3_ADDR; break;
    case 4: addr = EEPROM_DISPENSE4_ADDR; break;
    default: return 0;
  }
  EEPROM.begin(EEPROM_SIZE);
  uint8_t val = EEPROM.read(addr);
  EEPROM.end();
  return val;
}

// === Relay Prices === ✅ NEW
void saveRelayPriceToEEPROM(int relayNum, unsigned long price) {
  int addr;
  switch (relayNum) {
    case 1: addr = EEPROM_PRICE1_ADDR; break;
    case 2: addr = EEPROM_PRICE2_ADDR; break;
    case 3: addr = EEPROM_PRICE3_ADDR; break;
    case 4: addr = EEPROM_PRICE4_ADDR; break;
    default: return;
  }
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(addr, price);
  EEPROM.commit();
  EEPROM.end();
  relayPrices[relayNum - 1] = price;
}

unsigned long loadRelayPriceFromEEPROM(int relayNum) {
  unsigned long price = 0;
  int addr;
  switch (relayNum) {
    case 1: addr = EEPROM_PRICE1_ADDR; break;
    case 2: addr = EEPROM_PRICE2_ADDR; break;
    case 3: addr = EEPROM_PRICE3_ADDR; break;
    case 4: addr = EEPROM_PRICE4_ADDR; break;
    default: return 0;
  }
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(addr, price);
  EEPROM.end();
  relayPrices[relayNum - 1] = price;
  return price;
}

// === Clear EEPROM ===
void clearEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0xFF); // 0xFF = default erased state
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("✅ EEPROM cleared successfully.");
}
