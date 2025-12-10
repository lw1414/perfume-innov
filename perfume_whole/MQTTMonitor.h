#ifndef MQTT_MONITOR_H
#define MQTT_MONITOR_H

#include <Arduino.h>
#include "SystemConfig.h"
#include "MQTTHandler.h"

// Extern global MQTT handler (declared in main.ino)
extern MQTTHandler mqttHandler;

// === Public API ===
void startMQTTMonitorTask();
void publishRelayEventMQTT(int relayNum, int totalPesos, const char* state);

#endif
