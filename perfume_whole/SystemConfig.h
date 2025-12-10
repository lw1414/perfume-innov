#pragma once
#include <Arduino.h>
#include <EEPROM.h>

// === Shift Register Pins ===
#define OUT_CTRL_DIN 23
#define OUT_CTRL_CLK 25
#define OUT_CTRL_CS  26
#define BIT_OFF 1
#define BIT_ON  0
#define WDT_PIN 13

// === Input Pins ===
#define BUTTON1_PIN 32
#define BUTTON2_PIN 33
#define BUTTON3_PIN 34
#define BUTTON4_PIN 35
#define COIN_PIN    21

// === EEPROM SETTINGS ===
#define EEPROM_SIZE                  2048  // Expanded for safety

// === WiFi Credentials (96 bytes reserved) ===
#define EEPROM_WIFI_CRED_ADDR        0     // [0 – 95]

// === MQTT Config (160 bytes reserved) ===
#define EEPROM_MQTT_CONFIG_ADDR      128   // [128 – 287]

// === Device ESN (32 bytes reserved) ===
#define EEPROM_ESN_ADDR              288   // [288 – 319]

// === Relay Durations (4 relays × 4 bytes each = 16 bytes) ===
#define EEPROM_RELAY1_DURATION_ADDR  320
#define EEPROM_RELAY2_DURATION_ADDR  324
#define EEPROM_RELAY3_DURATION_ADDR  328
#define EEPROM_RELAY4_DURATION_ADDR  332

// === Send Interval (4 bytes) ===
#define EEPROM_SEND_INTERVAL_ADDR    336   // [336 – 339]

// === Dispense Status (4 bytes) ===
#define EEPROM_DISPENSE1_ADDR        340
#define EEPROM_DISPENSE2_ADDR        341
#define EEPROM_DISPENSE3_ADDR        342
#define EEPROM_DISPENSE4_ADDR        343

// === Prices (4 relays × 4 bytes each = 16 bytes) ===
#define EEPROM_PRICE1_ADDR           350
#define EEPROM_PRICE2_ADDR           354
#define EEPROM_PRICE3_ADDR           358
#define EEPROM_PRICE4_ADDR           362

// === Reserved for future expansion ===
#define EEPROM_RESERVED_ADDR         400

// === Device ID ===
#define DEVICE_ESN_MAX_LEN 32

// === Structures ===
typedef struct {
    char mqttServer[64];
    char mqttUser[32];
    char mqttPassword[32];
    int  mqttPort;
} MQTTConfig_t;

typedef struct {
    char ssid[32];
    char password[64];
} WiFiCreds_t;

// === Globals ===
extern MQTTConfig_t mqttConfig;
extern char deviceESN[DEVICE_ESN_MAX_LEN];
extern unsigned long relayDurations[4];
extern uint8_t dispenseStatus[4];
extern unsigned long relayPrices[4];   // ✅ NEW: price for each relay

// === MQTT Topics ===
extern String willTopic;
extern String willMessage;
extern String initTopic;
extern String intervalConfig;
extern String mqttConfiguration;
extern String topicRelayDuration;
extern String topicCurrentCredit;

// === Function Prototypes ===
void initSystemConfig();
void loadMQTTConfigFromEEPROM();
void saveMQTTConfigToEEPROM();
void loadDeviceESNFromEEPROM();
void saveDeviceESNToEEPROM();
void initializeDynamicTopics();
void saveWiFiCredsToEEPROM(const WiFiCreds_t& creds);
WiFiCreds_t loadWiFiCredsFromEEPROM();

// === Relay Duration ===
void saveRelayDurationToEEPROM(int relayNum, unsigned long duration);
unsigned long loadRelayDurationFromEEPROM(int relayNum);

// === Send Interval ===
void saveSendIntervalToEEPROM(uint32_t interval);
uint32_t loadSendIntervalFromEEPROM();

// === Dispense Status ===
void saveDispenseStatusToEEPROM(int relayNum, uint8_t status);
uint8_t loadDispenseStatusFromEEPROM(int relayNum);

// === Relay Prices === ✅ NEW
void saveRelayPriceToEEPROM(int relayNum, unsigned long price);
unsigned long loadRelayPriceFromEEPROM(int relayNum);
