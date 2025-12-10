#include "CLIHandler.h"
#include "SystemConfig.h"
#include "ShiftRegister.h"

extern ShiftRegister OUTPUT_CONTROL_PORT;
extern int totalPesos;
extern int totalPesosAccumulated;
extern unsigned long relayDurations[4];
extern char deviceESN[];

static String commandBuffer = "";

void CLIHandler::init() {
  commandBuffer.reserve(64);
}

void CLIHandler::handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      commandBuffer.trim();
      if (commandBuffer.length() > 0)
        processCommand(commandBuffer);
      commandBuffer = "";
    } else {
      commandBuffer += c;
    }
  }
}

void CLIHandler::processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // === Basic AT commands ===
  if (cmd.equalsIgnoreCase("AT+TOTAL?")) {
    Serial.printf("Total amount: ₱%d\n", totalPesos);
    return;
  }

  // === Relay duration ===
  if (cmd.startsWith("AT+RELAY")) {
    int relayNum = cmd.charAt(8) - '0';
    if (relayNum < 1 || relayNum > 4) {
      Serial.println("Invalid relay number (1-4).");
      return;
    }

    if (cmd.endsWith("?")) {
      Serial.printf("Relay %d duration = %lu ms\n", relayNum, relayDurations[relayNum - 1]);
    } else if (cmd.indexOf('=') > 0) {
      unsigned long val = cmd.substring(cmd.indexOf('=') + 1).toInt();
      relayDurations[relayNum - 1] = val;
      saveRelayDurationToEEPROM(relayNum, val);
      Serial.printf("Relay %d duration set to %lu ms\n", relayNum, val);
    }
    return;
  }

  // === Dispense status ===
  if (cmd.startsWith("AT+DISPENSE")) {
    int relayNum = cmd.charAt(11) - '0';
    if (relayNum < 1 || relayNum > 4) {
      Serial.println("Invalid relay number (1-4).");
      return;
    }

    if (cmd.endsWith("?")) {
      Serial.printf("Relay %d dispense status = %d\n", relayNum, dispenseStatus[relayNum - 1]);
    } else if (cmd.indexOf('=') > 0) {
      int val = cmd.substring(cmd.indexOf('=') + 1).toInt();
      dispenseStatus[relayNum - 1] = (val != 0);
      saveDispenseStatusToEEPROM(relayNum, dispenseStatus[relayNum - 1]);
      Serial.printf("Relay %d dispense status set to %d\n", relayNum, dispenseStatus[relayNum - 1]);
    }
    return;
  }

  // === WiFi credentials ===
  if (cmd.startsWith("AT+WIFI")) {
    if (cmd.endsWith("?")) {
      WiFiCreds_t creds = loadWiFiCredsFromEEPROM();
      Serial.printf("WiFi SSID='%s', PASS='%s'\n", creds.ssid, creds.password);
    } else if (cmd.indexOf('=') > 0) {
      String val = cmd.substring(cmd.indexOf('=') + 1);
      int sep = val.indexOf(',');
      if (sep > 0) {
        WiFiCreds_t creds;
        strncpy(creds.ssid, val.substring(0, sep).c_str(), sizeof(creds.ssid));
        strncpy(creds.password, val.substring(sep + 1).c_str(), sizeof(creds.password));
        saveWiFiCredsToEEPROM(creds);
        Serial.println("WiFi credentials saved.");
      }
    }
    return;
  }

  // === MQTT credentials ===
  if (cmd.startsWith("AT+MQTT")) {
    if (cmd.endsWith("?")) {
      Serial.printf("MQTT Server='%s', Port=%d, User='%s'\n",
                    mqttConfig.mqttServer, mqttConfig.mqttPort, mqttConfig.mqttUser);
    } else if (cmd.indexOf('=') > 0) {
      String val = cmd.substring(cmd.indexOf('=') + 1);
      int p1 = val.indexOf(','), p2 = val.indexOf(',', p1 + 1), p3 = val.indexOf(',', p2 + 1);
      if (p1 > 0 && p2 > 0 && p3 > 0) {
        strncpy(mqttConfig.mqttServer, val.substring(0, p1).c_str(), sizeof(mqttConfig.mqttServer));
        mqttConfig.mqttPort = val.substring(p1 + 1, p2).toInt();
        strncpy(mqttConfig.mqttUser, val.substring(p2 + 1, p3).c_str(), sizeof(mqttConfig.mqttUser));
        strncpy(mqttConfig.mqttPassword, val.substring(p3 + 1).c_str(), sizeof(mqttConfig.mqttPassword));
        saveMQTTConfigToEEPROM();
        Serial.println("MQTT config saved.");
      }
    }
    return;
  }

  // === ESN ===
  if (cmd.startsWith("AT+ESN")) {
    if (cmd.endsWith("?")) {
      Serial.printf("Device ESN: %s\n", deviceESN);
    } else if (cmd.indexOf('=') > 0) {
      String val = cmd.substring(cmd.indexOf('=') + 1);
      val.toCharArray(deviceESN, DEVICE_ESN_MAX_LEN);
      saveDeviceESNToEEPROM();
      Serial.printf("Device ESN saved: %s\n", deviceESN);
    }
    return;
  }

  // === Clear EEPROM Data ===
  if (cmd.equalsIgnoreCase("AT+CLEAR")) {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < EEPROM_SIZE; i++) {
      EEPROM.write(i, 0xFF);
    }
    EEPROM.commit();
    EEPROM.end();
    Serial.println("✅ All EEPROM data cleared successfully!");

    // Optional: Reinitialize defaults in RAM
    memset(deviceESN, 0, sizeof(deviceESN));
    memset(&mqttConfig, 0, sizeof(mqttConfig));
    memset(relayDurations, 0, sizeof(relayDurations));
    memset(dispenseStatus, 0, sizeof(dispenseStatus));
    Serial.println("⚙️ Memory variables reset. Please reboot or reconfigure.");
    return;
  }

  // === Relay Price ===
  if (cmd.startsWith("AT+PRICE")) {
    int relayNum = cmd.charAt(8) - '0';
    if (relayNum < 1 || relayNum > 4) {
      Serial.println("Invalid relay number (1-4).");
      return;
    }

    if (cmd.endsWith("?")) {
      Serial.printf("Relay %d price = ₱%lu\n", relayNum, relayPrices[relayNum - 1]);
    } else if (cmd.indexOf('=') > 0) {
      unsigned long val = cmd.substring(cmd.indexOf('=') + 1).toInt();
      relayPrices[relayNum - 1] = val;
      saveRelayPriceToEEPROM(relayNum, val);
      Serial.printf("Relay %d price set to ₱%lu\n", relayNum, val);
    }
    return;
  }

  if (cmd.equalsIgnoreCase("AT?")) {
    printHelp();
    return;
  }

  Serial.println("Unknown command. Type AT? for help.");
}

void CLIHandler::printHelp() {
  Serial.println(F("Available AT Commands:"));
  Serial.println(F("  AT+TOTAL?            - Display total inserted amount"));
  Serial.println(F("  AT+CLEAR             - Clear all EEPROM data and reset configuration"));
  Serial.println(F("  AT+RELAYn?           - Query relay n duration (1-4)"));
  Serial.println(F("  AT+RELAYn=duration   - Set relay n duration in ms (1-4)"));
  Serial.println(F("  AT+PRICEn?           - Query relay n price (1-4)"));
  Serial.println(F("  AT+PRICEn=value      - Set relay n price in pesos (1-4)"));
  Serial.println(F("  AT+DISPENSEn?        - Query relay n dispense status (1-4)"));
  Serial.println(F("  AT+DISPENSEn=x       - Set relay n dispense status (0 or 1)"));
  Serial.println(F("  AT+WIFI=SSID,PASS    - Save Wi-Fi credentials"));
  Serial.println(F("  AT+MQTT=server,port,user,password - Save MQTT credentials"));
  Serial.println(F("  AT+ESN=value         - Save Device ESN"));
}
