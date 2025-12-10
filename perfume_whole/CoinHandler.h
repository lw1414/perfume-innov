#ifndef COIN_HANDLER_H
#define COIN_HANDLER_H

#include <Arduino.h>
#include "SystemConfig.h"

// === Public API ===
void startCoinTask();
int getTotalPesos();
void resetTotalPesos();

#endif
